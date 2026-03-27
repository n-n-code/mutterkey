#include "audio/recording.h"
#include "transcription/transcriptionengine.h"
#include "transcription/transcriptionworker.h"
#include "transcription/whispercpptranscriber.h"

#include <QSignalSpy>
#include <QtTest/QTest>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace {

BackendCapabilities fakeCapabilities()
{
    return BackendCapabilities{
        .backendName = QStringLiteral("fake"),
        .runtimeDescription = QStringLiteral("fake runtime"),
        .supportedLanguages = {QStringLiteral("en"), QStringLiteral("fi")},
        .supportsAutoLanguage = true,
        .supportsTranslation = true,
        .supportsWarmup = true,
    };
}

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
    explicit FakeTranscriptionSession(TranscriptionResult result)
        : m_result(std::move(result))
    {
    }

    [[nodiscard]] QString backendName() const override
    {
        return QStringLiteral("fake");
    }

    bool warmup(RuntimeError *error) override
    {
        if (!m_warmupError.isOk()) {
            if (error != nullptr) {
                *error = m_warmupError;
            }
            return false;
        }
        return true;
    }

    [[nodiscard]] TranscriptionResult transcribe(const Recording &) override
    {
        return m_result;
    }

    void cancel() override
    {
        m_cancelled = true;
    }

    void setWarmupError(RuntimeError warmupError)
    {
        m_warmupError = std::move(warmupError);
    }

    [[nodiscard]] bool wasCancelled() const
    {
        return m_cancelled;
    }

private:
    TranscriptionResult m_result;
    RuntimeError m_warmupError;
    bool m_cancelled = false;
};

class FakeTranscriptionEngine final : public TranscriptionEngine
{
public:
    explicit FakeTranscriptionEngine(TranscriptionResult result)
        : m_results{std::move(result)}
    {
    }

    explicit FakeTranscriptionEngine(std::vector<TranscriptionResult> results)
        : m_results(std::move(results))
    {
    }

    [[nodiscard]] BackendCapabilities capabilities() const override
    {
        return fakeCapabilities();
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
        const int resultIndex = std::min<int>(m_createSessionCalls - 1, static_cast<int>(m_results.size()) - 1);
        return std::make_unique<FakeTranscriptionSession>(m_results.at(resultIndex));
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
    std::vector<TranscriptionResult> m_results;
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
    void whisperEngineSurfacesMissingModelAtLoadTime();
    void whisperRuntimeRejectsUnsupportedLanguage();
};

} // namespace

void TranscriptionWorkerTest::initTestCase()
{
    qRegisterMetaType<RuntimeError>("RuntimeError");
}

void TranscriptionWorkerTest::reportsInjectedBackendName()
{
    auto session = std::make_unique<FakeTranscriptionSession>(TranscriptionResult{.success = true, .text = {}});
    const TranscriptionWorker worker(std::move(session));

    QCOMPARE(worker.backendName(), QStringLiteral("fake"));
    QVERIFY(worker.capabilities().runtimeDescription.isEmpty());
}

void TranscriptionWorkerTest::emitsReadySignalForSuccessfulTranscription()
{
    auto session =
        std::make_unique<FakeTranscriptionSession>(TranscriptionResult{.success = true, .text = QStringLiteral("hello world")});
    TranscriptionWorker worker(std::move(session));
    const QSignalSpy readySpy(&worker, &TranscriptionWorker::transcriptionReady);
    const QSignalSpy failedSpy(&worker, &TranscriptionWorker::transcriptionFailed);

    worker.transcribe(Recording{});

    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(readySpy.at(0).at(0).toString(), QStringLiteral("hello world"));
}

void TranscriptionWorkerTest::emitsFailureSignalForFailedTranscription()
{
    auto session = std::make_unique<FakeTranscriptionSession>(TranscriptionResult{
        .success = false,
        .text = {},
        .error = RuntimeError{
            .code = RuntimeErrorCode::DecodeFailed,
            .message = QStringLiteral("decode failed"),
        },
    });
    TranscriptionWorker worker(std::move(session));
    const QSignalSpy readySpy(&worker, &TranscriptionWorker::transcriptionReady);
    const QSignalSpy failedSpy(&worker, &TranscriptionWorker::transcriptionFailed);

    worker.transcribe(Recording{});

    QCOMPARE(readySpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
    const auto error = failedSpy.at(0).at(0).value<RuntimeError>();
    QCOMPARE(error.code, RuntimeErrorCode::DecodeFailed);
    QCOMPARE(error.message, QStringLiteral("decode failed"));
}

void TranscriptionWorkerTest::surfacesWarmupFailure()
{
    auto session = std::make_unique<FakeTranscriptionSession>(TranscriptionResult{.success = true, .text = {}});
    session->setWarmupError(RuntimeError{
        .code = RuntimeErrorCode::ModelLoadFailed,
        .message = QStringLiteral("model unavailable"),
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
        TranscriptionResult{.success = true, .text = QStringLiteral("engine path")});
    TranscriptionWorker worker(engine);
    const QSignalSpy readySpy(&worker, &TranscriptionWorker::transcriptionReady);

    QCOMPARE(engine->loadModelCalls(), 0);
    QCOMPARE(engine->createSessionCalls(), 0);
    QCOMPARE(worker.backendName(), QStringLiteral("fake"));

    worker.transcribe(Recording{});

    QCOMPARE(engine->loadModelCalls(), 1);
    QCOMPARE(engine->createSessionCalls(), 1);
    QVERIFY(engine->lastCreateSessionHadModel());
    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).toString(), QStringLiteral("engine path"));
}

void TranscriptionWorkerTest::recreatesSessionAfterTerminalFailureWithoutReloadingModel()
{
    const std::shared_ptr<FakeTranscriptionEngine> engine = std::make_shared<FakeTranscriptionEngine>(
        std::vector<TranscriptionResult>{
            TranscriptionResult{
                .success = false,
                .text = {},
                .error = RuntimeError{
                    .code = RuntimeErrorCode::DecodeFailed,
                    .message = QStringLiteral("decode failed"),
                },
            },
            TranscriptionResult{
                .success = true,
                .text = QStringLiteral("recovered"),
            },
        });
    TranscriptionWorker worker(engine);
    const QSignalSpy readySpy(&worker, &TranscriptionWorker::transcriptionReady);
    const QSignalSpy failedSpy(&worker, &TranscriptionWorker::transcriptionFailed);

    worker.transcribe(Recording{});
    worker.transcribe(Recording{});

    QCOMPARE(engine->loadModelCalls(), 1);
    QCOMPARE(engine->createSessionCalls(), 2);
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).toString(), QStringLiteral("recovered"));
}

void TranscriptionWorkerTest::whisperEngineSurfacesMissingModelAtLoadTime()
{
    TranscriberConfig config;
    config.modelPath = QStringLiteral("/tmp/definitely-missing-mutterkey-model.bin");

    const std::shared_ptr<const TranscriptionEngine> engine = createTranscriptionEngine(config);
    RuntimeError error;
    const std::shared_ptr<const TranscriptionModelHandle> model = engine->loadModel(&error);

    QVERIFY(model == nullptr);
    QCOMPARE(error.code, RuntimeErrorCode::ModelNotFound);
    QVERIFY(error.message.contains(QStringLiteral("Embedded Whisper model not found")));
}

void TranscriptionWorkerTest::whisperRuntimeRejectsUnsupportedLanguage()
{
    TranscriberConfig config;
    config.modelPath = QStringLiteral("/tmp/unused.bin");
    config.language = QStringLiteral("pirate");
    const std::shared_ptr<const TranscriptionModelHandle> model = std::make_shared<FakeModelHandle>();
    WhisperCppTranscriber transcriber(config, model);

    const TranscriptionResult result = transcriber.transcribe(Recording{});

    QVERIFY(!result.success);
    QCOMPARE(result.error.code, RuntimeErrorCode::UnsupportedLanguage);
    QVERIFY(result.error.message.contains(QStringLiteral("pirate")));
}

QTEST_APPLESS_MAIN(TranscriptionWorkerTest)

#include "transcriptionworkertest.moc"
