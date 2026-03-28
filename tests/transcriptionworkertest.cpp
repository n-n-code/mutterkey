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
        .selectionReason = QStringLiteral("fake selection"),
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

    [[nodiscard]] ModelMetadata metadata() const override
    {
        return ModelMetadata{
            .packageId = QStringLiteral("fake-model"),
            .displayName = QStringLiteral("Fake Model"),
            .runtimeFamily = QStringLiteral("asr"),
            .sourceFormat = QStringLiteral("fake"),
            .modelFormat = QStringLiteral("fake"),
        };
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
    void runtimeDiagnosticsIncludeLoadedModelDescriptionAfterFirstUse();
    void warmupFailureDiscardsSessionAndRecreatesItWithoutReloadingModel();
    void nonTerminalFailureKeepsReusableSession();
    void recreatesSessionAfterTerminalFailureWithoutReloadingModel();
};

} // namespace

void TranscriptionWorkerTest::initTestCase()
{
    qRegisterMetaType<RuntimeError>("RuntimeError");
}

void TranscriptionWorkerTest::reportsInjectedBackendName()
{
    // WHAT: Verify that a worker built from an injected live session exposes that backend
    // identity without requiring an engine.
    // HOW: Construct the worker with a fake session and inspect the reported backend name
    // and runtime-diagnostics payload.
    // WHY: Worker diagnostics are used before transcription begins, so they must stay useful
    // even in narrow tests and injected-session runtime paths.
    auto session = std::make_unique<FakeTranscriptionSession>(FakeSessionScript{});
    const TranscriptionWorker worker(std::move(session));

    QCOMPARE(worker.backendName(), QStringLiteral("fake"));
    QVERIFY(worker.runtimeDiagnostics().runtimeDescription.isEmpty());
}

void TranscriptionWorkerTest::emitsReadySignalForSuccessfulTranscription()
{
    // WHAT: Verify that a successful transcription emits the ready signal with final text.
    // HOW: Use a fake session that produces a partial update during streaming and a final
    // event during finish, then assert that only the ready signal fires.
    // WHY: This is the worker's main success contract for the service and UI orchestration
    // layers, so the final transcript must emerge on the expected signal.
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
    // WHAT: Verify that a failed transcription emits the failure signal with the runtime
    // error returned by the backend session.
    // HOW: Use a fake session whose finish step returns a decode failure and confirm that no
    // ready signal is emitted and the failure payload is preserved.
    // WHY: Runtime orchestration depends on stable structured errors to distinguish backend
    // failures from successful empty or partial transcript paths.
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
    // WHAT: Verify that worker warmup surfaces a backend warmup failure.
    // HOW: Inject a fake session whose warmup returns a model-load error and check that the
    // worker returns false with the same structured error.
    // WHY: Warmup is the preflight contract for daemon startup and diagnostics, so failures
    // must stay explicit before any recording is attempted.
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
    // WHAT: Verify that an engine-backed worker delays model loading and session creation
    // until transcription is actually requested.
    // HOW: Construct the worker with a fake engine, inspect the zero-call state, run one
    // transcription, and then check the engine interaction counts.
    // WHY: Lazy session creation keeps startup light and avoids creating mutable backend
    // state before the worker actually needs it on the runtime path.
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

void TranscriptionWorkerTest::runtimeDiagnosticsIncludeLoadedModelDescriptionAfterFirstUse()
{
    // WHAT: Verify that worker runtime diagnostics include the loaded model description after
    // the engine path has created a model-backed session.
    // HOW: Read diagnostics before and after the first successful transcription and confirm
    // that the loaded-model description appears only after model loading.
    // WHY: Operators rely on diagnostics to confirm which model is active, so the worker
    // must surface that information once the runtime has actually loaded it.
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

    QVERIFY(worker.runtimeDiagnostics().loadedModelDescription.isEmpty());

    worker.transcribeRecordingCompat(validRecording());

    QCOMPARE(worker.runtimeDiagnostics().loadedModelDescription, QStringLiteral("fake-model"));
}

void TranscriptionWorkerTest::warmupFailureDiscardsSessionAndRecreatesItWithoutReloadingModel()
{
    // WHAT: Verify that a terminal warmup failure discards the live session but keeps the
    // already loaded model for the next session creation attempt.
    // HOW: Warm up through an engine whose first session fails with a model-load error and
    // whose second session succeeds, then compare model-load and session-creation counts.
    // WHY: Recovery after a terminal session failure should be cheap and deterministic, not
    // force an unnecessary model reload or leave a poisoned session alive.
    const std::shared_ptr<FakeTranscriptionEngine> engine = std::make_shared<FakeTranscriptionEngine>(
        std::vector<FakeSessionScript>{
            FakeSessionScript{
                .pushUpdates = {},
                .finishUpdate = {},
                .warmupError = RuntimeError{
                    .code = RuntimeErrorCode::ModelLoadFailed,
                    .message = QStringLiteral("model unavailable"),
                },
            },
            FakeSessionScript{
                .pushUpdates = {},
                .finishUpdate = {},
                .warmupError = {},
            },
        });
    TranscriptionWorker worker(engine);

    RuntimeError error;
    QVERIFY(!worker.warmup(&error));
    QCOMPARE(error.code, RuntimeErrorCode::ModelLoadFailed);
    QVERIFY(worker.warmup(&error));

    QCOMPARE(engine->loadModelCalls(), 1);
    QCOMPARE(engine->createSessionCalls(), 2);
}

void TranscriptionWorkerTest::nonTerminalFailureKeepsReusableSession()
{
    // WHAT: Verify that a non-terminal transcription failure keeps the current session alive
    // for later reuse.
    // HOW: Use an engine-backed worker, trigger an audio-normalization failure with an empty
    // recording, then run a valid transcription and confirm the same session was reused.
    // WHY: Input-side failures should not force backend-session recreation, because that
    // would add unnecessary work and make recovery less predictable.
    const std::shared_ptr<FakeTranscriptionEngine> engine = std::make_shared<FakeTranscriptionEngine>(
        FakeSessionScript{
            .pushUpdates = {},
            .finishUpdate = TranscriptUpdate{
                .events = {TranscriptEvent{
                    .kind = TranscriptEventKind::Final,
                    .text = QStringLiteral("reused"),
                }},
                .error = {},
            },
            .warmupError = {},
        });
    TranscriptionWorker worker(engine);
    const QSignalSpy readySpy(&worker, &TranscriptionWorker::transcriptionReady);
    const QSignalSpy failedSpy(&worker, &TranscriptionWorker::transcriptionFailed);

    worker.transcribeRecordingCompat(Recording{});
    worker.transcribeRecordingCompat(validRecording());

    QCOMPARE(engine->loadModelCalls(), 1);
    QCOMPARE(engine->createSessionCalls(), 1);
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).toString(), QStringLiteral("reused"));
    const auto error = failedSpy.at(0).at(0).value<RuntimeError>();
    QCOMPARE(error.code, RuntimeErrorCode::AudioNormalizationFailed);
}

void TranscriptionWorkerTest::recreatesSessionAfterTerminalFailureWithoutReloadingModel()
{
    // WHAT: Verify that a terminal decode failure causes the worker to recreate the session
    // while reusing the already loaded model.
    // HOW: Run two transcriptions through an engine whose first session fails at finish and
    // whose second session succeeds, then inspect the engine interaction counts.
    // WHY: This is the worker's recovery path after a poisoned decode session, and it must
    // restore service operation without paying the cost of a second model load.
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
