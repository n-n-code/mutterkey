#include "audio/recording.h"
#include "transcription/transcriptionengine.h"
#include "transcription/transcriptionworker.h"

#include <QAudioFormat>
#include <QSignalSpy>
#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <memory>
#include <utility>
#include <vector>

namespace {

void appendSample(QByteArray *pcmData, qint16 sample)
{
    const auto sampleBits = static_cast<quint16>(sample);
    pcmData->append(static_cast<char>(static_cast<quint32>(sampleBits) & 0x00ffU));
    pcmData->append(static_cast<char>((static_cast<quint32>(sampleBits) >> 8U) & 0x00ffU));
}

BackendCapabilities fakeCapabilities()
{
    return BackendCapabilities{
        .backendName = QStringLiteral("fake"),
        .supportedLanguages = {QStringLiteral("en"), QStringLiteral("fi")},
        .supportsAutoLanguage = true,
        .supportsTranslation = true,
        .supportsWarmup = true,
    };
}

RuntimeDiagnostics fakeDiagnostics()
{
    return RuntimeDiagnostics{
        .backendName = QStringLiteral("fake"),
        .runtimeDescription = QStringLiteral("fake runtime"),
        .loadedModelDescription = {},
    };
}

Recording validRecording()
{
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    Recording recording;
    recording.format = format;
    recording.durationSeconds = 0.25;

    const std::array<qint16, 8> samples{0, 1024, -512, 256, -128, 64, -32, 16};
    for (const qint16 sample : samples) {
        appendSample(&recording.pcmData, sample);
    }
    return recording;
}

struct FakeSessionScript {
    std::vector<TranscriptUpdate> pushUpdates;
    TranscriptUpdate finishUpdate;
    RuntimeError warmupError;
};

class FakeModelHandle final : public TranscriptionModelHandle
{
public:
    [[nodiscard]] QString backendName() const override
    {
        return QStringLiteral("fake");
    }

    [[nodiscard]] QString modelDescription() const override
    {
        return QStringLiteral("fake-model");
    }
};

class FakeTranscriptionSession final : public TranscriptionSession
{
public:
    explicit FakeTranscriptionSession(FakeSessionScript script)
        : m_script(std::move(script))
    {
    }

    [[nodiscard]] QString backendName() const override
    {
        return QStringLiteral("fake");
    }

    bool warmup(RuntimeError *error) override
    {
        if (!m_script.warmupError.isOk()) {
            if (error != nullptr) {
                *error = m_script.warmupError;
            }
            return false;
        }
        return true;
    }

    [[nodiscard]] TranscriptUpdate pushAudioChunk(const AudioChunk &chunk) override
    {
        m_pushedChunkCount += 1;
        m_pushedSampleCount += static_cast<int>(chunk.samples.size());
        if (std::cmp_less(static_cast<std::size_t>(m_pushIndex), m_script.pushUpdates.size())) {
            return m_script.pushUpdates.at(static_cast<std::size_t>(m_pushIndex++));
        }
        return {};
    }

    [[nodiscard]] TranscriptUpdate finish() override
    {
        m_finishCount += 1;
        return m_script.finishUpdate;
    }

    [[nodiscard]] TranscriptUpdate cancel() override
    {
        m_cancelled = true;
        return TranscriptUpdate{
            .events = {},
            .error = RuntimeError{
                .code = RuntimeErrorCode::Cancelled,
                .message = QStringLiteral("cancelled"),
            },
        };
    }

    [[nodiscard]] int pushedChunkCount() const
    {
        return m_pushedChunkCount;
    }

    [[nodiscard]] int pushedSampleCount() const
    {
        return m_pushedSampleCount;
    }

    [[nodiscard]] int finishCount() const
    {
        return m_finishCount;
    }

    [[nodiscard]] bool wasCancelled() const
    {
        return m_cancelled;
    }

private:
    FakeSessionScript m_script;
    int m_pushIndex = 0;
    int m_pushedChunkCount = 0;
    int m_pushedSampleCount = 0;
    int m_finishCount = 0;
    bool m_cancelled = false;
};

class FakeTranscriptionEngine final : public TranscriptionEngine
{
public:
    explicit FakeTranscriptionEngine(FakeSessionScript script)
        : m_scripts{std::move(script)}
    {
    }

    explicit FakeTranscriptionEngine(std::vector<FakeSessionScript> scripts)
        : m_scripts(std::move(scripts))
    {
    }

    [[nodiscard]] BackendCapabilities capabilities() const override
    {
        return fakeCapabilities();
    }

    [[nodiscard]] RuntimeDiagnostics diagnostics() const override
    {
        return fakeDiagnostics();
    }

    [[nodiscard]] std::shared_ptr<const TranscriptionModelHandle> loadModel(RuntimeError *error) const override
    {
        Q_UNUSED(error);
        ++m_loadModelCalls;
        if (m_model == nullptr) {
            m_model = std::make_shared<FakeModelHandle>();
        }
        return m_model;
    }

    [[nodiscard]] std::unique_ptr<TranscriptionSession>
    createSession(std::shared_ptr<const TranscriptionModelHandle> model) const override
    {
        ++m_createSessionCalls;
        m_lastModel = std::move(model);
        const int scriptIndex = std::min<int>(m_createSessionCalls - 1, static_cast<int>(m_scripts.size()) - 1);
        return std::make_unique<FakeTranscriptionSession>(m_scripts.at(static_cast<std::size_t>(scriptIndex)));
    }

    [[nodiscard]] int loadModelCalls() const
    {
        return m_loadModelCalls;
    }

    [[nodiscard]] int createSessionCalls() const
    {
        return m_createSessionCalls;
    }

    [[nodiscard]] bool lastCreateSessionHadModel() const
    {
        return m_lastModel != nullptr;
    }

private:
    std::vector<FakeSessionScript> m_scripts;
    mutable int m_loadModelCalls = 0;
    mutable int m_createSessionCalls = 0;
    mutable std::shared_ptr<const TranscriptionModelHandle> m_model;
    mutable std::shared_ptr<const TranscriptionModelHandle> m_lastModel;
};

class TranscriptionWorkerTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void reportsInjectedBackendName();
    void emitsReadySignalForSuccessfulTranscription();
    void emitsFailureSignalForFailedTranscription();
    void surfacesWarmupFailure();
    void lazilyCreatesSessionFromInjectedEngine();
    void recreatesSessionAfterTerminalFailureWithoutReloadingModel();
};

} // namespace

void TranscriptionWorkerTest::initTestCase()
{
    qRegisterMetaType<RuntimeError>("RuntimeError");
}

void TranscriptionWorkerTest::reportsInjectedBackendName()
{
    auto session = std::make_unique<FakeTranscriptionSession>(FakeSessionScript{});
    const TranscriptionWorker worker(std::move(session));

    QCOMPARE(worker.backendName(), QStringLiteral("fake"));
    QVERIFY(worker.runtimeDiagnostics().runtimeDescription.isEmpty());
}

void TranscriptionWorkerTest::emitsReadySignalForSuccessfulTranscription()
{
    auto session = std::make_unique<FakeTranscriptionSession>(FakeSessionScript{
        .pushUpdates = {
            TranscriptUpdate{
                .events = {TranscriptEvent{
                    .kind = TranscriptEventKind::Partial,
                    .text = QStringLiteral("hello"),
                }},
                .error = {},
            },
        },
        .finishUpdate = TranscriptUpdate{
            .events = {TranscriptEvent{
                .kind = TranscriptEventKind::Final,
                .text = QStringLiteral("hello world"),
            }},
            .error = {},
        },
        .warmupError = {},
    });
    TranscriptionWorker worker(std::move(session));
    const QSignalSpy readySpy(&worker, &TranscriptionWorker::transcriptionReady);
    const QSignalSpy failedSpy(&worker, &TranscriptionWorker::transcriptionFailed);

    worker.transcribeRecordingCompat(validRecording());

    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(readySpy.at(0).at(0).toString(), QStringLiteral("hello world"));
}

void TranscriptionWorkerTest::emitsFailureSignalForFailedTranscription()
{
    auto session = std::make_unique<FakeTranscriptionSession>(FakeSessionScript{
        .pushUpdates = {},
        .finishUpdate = TranscriptUpdate{
            .events = {},
            .error = RuntimeError{
                .code = RuntimeErrorCode::DecodeFailed,
                .message = QStringLiteral("decode failed"),
            },
        },
        .warmupError = {},
    });
    TranscriptionWorker worker(std::move(session));
    const QSignalSpy readySpy(&worker, &TranscriptionWorker::transcriptionReady);
    const QSignalSpy failedSpy(&worker, &TranscriptionWorker::transcriptionFailed);

    worker.transcribeRecordingCompat(validRecording());

    QCOMPARE(readySpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
    const auto error = failedSpy.at(0).at(0).value<RuntimeError>();
    QCOMPARE(error.code, RuntimeErrorCode::DecodeFailed);
    QCOMPARE(error.message, QStringLiteral("decode failed"));
}

void TranscriptionWorkerTest::surfacesWarmupFailure()
{
    auto session = std::make_unique<FakeTranscriptionSession>(FakeSessionScript{
        .pushUpdates = {},
        .finishUpdate = {},
        .warmupError = RuntimeError{
            .code = RuntimeErrorCode::ModelLoadFailed,
            .message = QStringLiteral("model unavailable"),
        },
    });
    TranscriptionWorker worker(std::move(session));

    RuntimeError error;
    QVERIFY(!worker.warmup(&error));
    QCOMPARE(error.code, RuntimeErrorCode::ModelLoadFailed);
    QCOMPARE(error.message, QStringLiteral("model unavailable"));
}

void TranscriptionWorkerTest::lazilyCreatesSessionFromInjectedEngine()
{
    const std::shared_ptr<FakeTranscriptionEngine> engine = std::make_shared<FakeTranscriptionEngine>(
        FakeSessionScript{
            .pushUpdates = {},
            .finishUpdate = TranscriptUpdate{
                .events = {TranscriptEvent{
                    .kind = TranscriptEventKind::Final,
                    .text = QStringLiteral("engine path"),
                }},
                .error = {},
            },
            .warmupError = {},
        });
    TranscriptionWorker worker(engine);
    const QSignalSpy readySpy(&worker, &TranscriptionWorker::transcriptionReady);

    QCOMPARE(engine->loadModelCalls(), 0);
    QCOMPARE(engine->createSessionCalls(), 0);
    QCOMPARE(worker.backendName(), QStringLiteral("fake"));

    worker.transcribeRecordingCompat(validRecording());

    QCOMPARE(engine->loadModelCalls(), 1);
    QCOMPARE(engine->createSessionCalls(), 1);
    QVERIFY(engine->lastCreateSessionHadModel());
    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).toString(), QStringLiteral("engine path"));
}

void TranscriptionWorkerTest::recreatesSessionAfterTerminalFailureWithoutReloadingModel()
{
    const std::shared_ptr<FakeTranscriptionEngine> engine = std::make_shared<FakeTranscriptionEngine>(
        std::vector<FakeSessionScript>{
            FakeSessionScript{
                .pushUpdates = {},
                .finishUpdate = TranscriptUpdate{
                    .events = {},
                    .error = RuntimeError{
                        .code = RuntimeErrorCode::DecodeFailed,
                        .message = QStringLiteral("decode failed"),
                    },
                },
                .warmupError = {},
            },
            FakeSessionScript{
                .pushUpdates = {},
                .finishUpdate = TranscriptUpdate{
                    .events = {TranscriptEvent{
                        .kind = TranscriptEventKind::Final,
                        .text = QStringLiteral("recovered"),
                    }},
                    .error = {},
                },
                .warmupError = {},
            },
        });
    TranscriptionWorker worker(engine);
    const QSignalSpy readySpy(&worker, &TranscriptionWorker::transcriptionReady);
    const QSignalSpy failedSpy(&worker, &TranscriptionWorker::transcriptionFailed);

    worker.transcribeRecordingCompat(validRecording());
    worker.transcribeRecordingCompat(validRecording());

    QCOMPARE(engine->loadModelCalls(), 1);
    QCOMPARE(engine->createSessionCalls(), 2);
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).toString(), QStringLiteral("recovered"));
}

QTEST_APPLESS_MAIN(TranscriptionWorkerTest)

#include "transcriptionworkertest.moc"
