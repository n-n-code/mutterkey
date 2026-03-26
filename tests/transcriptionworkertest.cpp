#include "audio/recording.h"
#include "transcription/transcriptionengine.h"
#include "transcription/transcriptionworker.h"

#include <QSignalSpy>
#include <QtTest/QTest>

#include <memory>
#include <utility>

namespace {

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

    bool warmup(QString *errorMessage) override
    {
        if (!m_warmupError.isEmpty() && errorMessage != nullptr) {
            *errorMessage = m_warmupError;
        }
        return m_warmupError.isEmpty();
    }

    [[nodiscard]] TranscriptionResult transcribe(const Recording &) override
    {
        return m_result;
    }

    void setWarmupError(QString warmupError)
    {
        m_warmupError = std::move(warmupError);
    }

private:
    TranscriptionResult m_result;
    QString m_warmupError;
};

class TranscriptionWorkerTest final : public QObject
{
    Q_OBJECT

private slots:
    void reportsInjectedBackendName();
    void emitsReadySignalForSuccessfulTranscription();
    void emitsFailureSignalForFailedTranscription();
    void surfacesWarmupFailure();
};

} // namespace

void TranscriptionWorkerTest::reportsInjectedBackendName()
{
    auto session = std::make_unique<FakeTranscriptionSession>(TranscriptionResult{.success = true, .text = {}});
    const TranscriptionWorker worker(std::move(session));

    QCOMPARE(worker.backendName(), QStringLiteral("fake"));
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
    auto session = std::make_unique<FakeTranscriptionSession>(
        TranscriptionResult{.success = false, .text = {}, .error = QStringLiteral("decode failed")});
    TranscriptionWorker worker(std::move(session));
    const QSignalSpy readySpy(&worker, &TranscriptionWorker::transcriptionReady);
    const QSignalSpy failedSpy(&worker, &TranscriptionWorker::transcriptionFailed);

    worker.transcribe(Recording{});

    QCOMPARE(readySpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.at(0).at(0).toString(), QStringLiteral("decode failed"));
}

void TranscriptionWorkerTest::surfacesWarmupFailure()
{
    auto session = std::make_unique<FakeTranscriptionSession>(TranscriptionResult{.success = true, .text = {}});
    session->setWarmupError(QStringLiteral("model unavailable"));
    TranscriptionWorker worker(std::move(session));

    QString errorMessage;
    QVERIFY(!worker.warmup(&errorMessage));
    QCOMPARE(errorMessage, QStringLiteral("model unavailable"));
}

QTEST_APPLESS_MAIN(TranscriptionWorkerTest)

#include "transcriptionworkertest.moc"
