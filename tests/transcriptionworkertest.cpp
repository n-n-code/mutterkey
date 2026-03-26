#include "audio/recording.h"
#include "transcription/transcriptionengine.h"
#include "transcription/transcriptionworker.h"
#include "transcription/whispercpptranscriber.h"

#include <QSignalSpy>
#include <QtTest/QTest>

#include <memory>
#include <utility>

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

    void setWarmupError(RuntimeError warmupError)
    {
        m_warmupError = std::move(warmupError);
    }

private:
    TranscriptionResult m_result;
    RuntimeError m_warmupError;
};

class FakeTranscriptionEngine final : public TranscriptionEngine
{
public:
    explicit FakeTranscriptionEngine(std::unique_ptr<FakeTranscriptionSession> session)
        : m_session(std::move(session))
    {
    }

    [[nodiscard]] BackendCapabilities capabilities() const override
    {
        return fakeCapabilities();
    }

    [[nodiscard]] std::unique_ptr<TranscriptionSession> createSession() const override
    {
        ++m_createSessionCalls;
        return std::move(m_session);
    }

    [[nodiscard]] int createSessionCalls() const
    {
        return m_createSessionCalls;
    }

private:
    mutable int m_createSessionCalls = 0;
    mutable std::unique_ptr<FakeTranscriptionSession> m_session;
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
    auto session =
        std::make_unique<FakeTranscriptionSession>(TranscriptionResult{.success = true, .text = QStringLiteral("engine path")});
    const std::shared_ptr<FakeTranscriptionEngine> engine = std::make_shared<FakeTranscriptionEngine>(std::move(session));
    TranscriptionWorker worker(engine);
    const QSignalSpy readySpy(&worker, &TranscriptionWorker::transcriptionReady);

    QCOMPARE(engine->createSessionCalls(), 0);
    QCOMPARE(worker.backendName(), QStringLiteral("fake"));

    worker.transcribe(Recording{});

    QCOMPARE(engine->createSessionCalls(), 1);
    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).toString(), QStringLiteral("engine path"));
}

void TranscriptionWorkerTest::whisperRuntimeRejectsUnsupportedLanguage()
{
    TranscriberConfig config;
    config.modelPath = QStringLiteral("/tmp/unused.bin");
    config.language = QStringLiteral("pirate");
    WhisperCppTranscriber transcriber(config);

    const TranscriptionResult result = transcriber.transcribe(Recording{});

    QVERIFY(!result.success);
    QCOMPARE(result.error.code, RuntimeErrorCode::UnsupportedLanguage);
    QVERIFY(result.error.message.contains(QStringLiteral("pirate")));
}

QTEST_APPLESS_MAIN(TranscriptionWorkerTest)

#include "transcriptionworkertest.moc"
