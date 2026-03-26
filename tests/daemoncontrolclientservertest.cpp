#include "control/daemoncontrolclient.h"
#include "control/daemoncontrolserver.h"

#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>
#include <QtTest/QTest>

#include <cstdint>
#include <memory>

namespace {

QString uniqueSocketName()
{
    return QStringLiteral("mutterkey-test-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

class SessionFetchWorker final : public QObject
{
    Q_OBJECT

public:
    enum class Operation : std::uint8_t {
        FetchStatus,
        FetchConfig,
    };

    explicit SessionFetchWorker(QString socketName, Operation operation)
        : m_socketName(std::move(socketName))
        , m_operation(operation)
    {
    }

    [[nodiscard]] const DaemonStatusResult &statusResult() const { return m_statusResult; }
    [[nodiscard]] const DaemonConfigResult &configResult() const { return m_configResult; }

public Q_SLOTS:
    void run()
    {
        const LocalDaemonControlSession session(m_socketName);
        if (m_operation == Operation::FetchStatus) {
            m_statusResult = session.fetchStatus(2000);
        } else {
            m_configResult = session.fetchConfig(2000);
        }
        emit finished();
    }

Q_SIGNALS:
    void finished();

private:
    QString m_socketName;
    Operation m_operation = Operation::FetchStatus;
    DaemonStatusResult m_statusResult;
    DaemonConfigResult m_configResult;
};

class SessionFetchThread final
{
public:
    SessionFetchThread(QString socketName, SessionFetchWorker::Operation operation)
        : m_worker(std::move(socketName), operation)
    {
        m_worker.moveToThread(&m_thread);
    }

    ~SessionFetchThread()
    {
        m_thread.quit();
        m_thread.wait(2000);
    }

    SessionFetchThread(const SessionFetchThread &) = delete;
    SessionFetchThread &operator=(const SessionFetchThread &) = delete;
    SessionFetchThread(SessionFetchThread &&) = delete;
    SessionFetchThread &operator=(SessionFetchThread &&) = delete;

    void start()
    {
        if (m_started) {
            return;
        }

        QObject::connect(&m_worker, &SessionFetchWorker::finished, &m_thread, &QThread::quit, Qt::UniqueConnection);
        m_finishedSpy = std::make_unique<QSignalSpy>(&m_worker, &SessionFetchWorker::finished);
        m_thread.start();
        QVERIFY(QMetaObject::invokeMethod(&m_worker, &SessionFetchWorker::run, Qt::QueuedConnection));
        m_started = true;
    }

    void wait()
    {
        QVERIFY(m_started);
        QTRY_COMPARE_WITH_TIMEOUT(m_finishedSpy->count(), 1, 2000);
        QVERIFY(m_thread.wait(2000));
    }

    void runAndWait()
    {
        start();
        wait();
    }

    [[nodiscard]] const DaemonStatusResult &statusResult() const { return m_worker.statusResult(); }
    [[nodiscard]] const DaemonConfigResult &configResult() const { return m_worker.configResult(); }

private:
    QThread m_thread;
    SessionFetchWorker m_worker;
    std::unique_ptr<QSignalSpy> m_finishedSpy;
    bool m_started = false;
};

class DaemonControlClientServerTest final : public QObject
{
    Q_OBJECT

private slots:
    void fetchStatusRoundTripsThroughLocalServer();
    void fetchConfigRoundTripsThroughLocalServer();
    void fetchStatusReportsMismatchedResponseId();
    void fetchStatusReportsMalformedSnapshotPayload();
};

} // namespace

void DaemonControlClientServerTest::fetchStatusRoundTripsThroughLocalServer()
{
    // WHAT: Verify that the local client can fetch daemon status from the real local server.
    // HOW: Start a test server on an injected socket name, fetch status through the real client,
    // and confirm that the typed status result contains the expected config-path metadata.
    // WHY: This is the smallest end-to-end proof that the client, socket transport, protocol,
    // and typed status parsing all work together as one control-plane path.
    const QString socketName = uniqueSocketName();
    const QString configPath = QStringLiteral("/tmp/test-mutterkey-config.json");
    DaemonControlServer server(configPath, defaultAppConfig(), nullptr, socketName, nullptr);

    QString errorMessage;
    QVERIFY2(server.start(&errorMessage), qPrintable(errorMessage));
    const auto stopServer = qScopeGuard([&server]() { server.stop(); });

    SessionFetchThread fetchThread(socketName, SessionFetchWorker::Operation::FetchStatus);
    fetchThread.runAndWait();

    const DaemonStatusResult result = fetchThread.statusResult();

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QVERIFY(result.errorMessage.isEmpty());
    QVERIFY(result.snapshot.daemonRunning);
    QCOMPARE(result.snapshot.configPath, configPath);
    QCOMPARE(result.snapshot.configExists, false);
    QVERIFY(result.snapshot.serviceDiagnostics.isEmpty());
}

void DaemonControlClientServerTest::fetchConfigRoundTripsThroughLocalServer()
{
    // WHAT: Verify that the local client can fetch daemon config from the real local server.
    // HOW: Start a test server with a custom config snapshot, fetch config through the real
    // client, and compare the parsed snapshot fields with the injected server-side values.
    // WHY: This proves the config inspection path works across the real transport boundary,
    // which is important for tray and CLI tooling that depend on daemon-owned config state.
    const QString socketName = uniqueSocketName();
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    AppConfig config = defaultAppConfig();
    config.shortcut.sequence = QStringLiteral("Meta+F8");
    config.transcriber.modelPath = tempDir.filePath(QStringLiteral("model.bin"));
    const QString configPath = tempDir.filePath(QStringLiteral("config.json"));
    QFile configFile(configPath);
    QVERIFY(configFile.open(QIODevice::WriteOnly | QIODevice::Text));
    configFile.write("{}");
    configFile.close();

    DaemonControlServer server(configPath, config, nullptr, socketName, nullptr);
    QString errorMessage;
    QVERIFY2(server.start(&errorMessage), qPrintable(errorMessage));
    const auto stopServer = qScopeGuard([&server]() { server.stop(); });

    SessionFetchThread fetchThread(socketName, SessionFetchWorker::Operation::FetchConfig);
    fetchThread.runAndWait();

    const DaemonConfigResult result = fetchThread.configResult();

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QVERIFY(result.errorMessage.isEmpty());
    QCOMPARE(result.snapshot.configPath, configPath);
    QCOMPARE(result.snapshot.configExists, true);
    QCOMPARE(result.snapshot.config.shortcut.sequence, QStringLiteral("Meta+F8"));
    QCOMPARE(result.snapshot.config.transcriber.modelPath, config.transcriber.modelPath);
}

void DaemonControlClientServerTest::fetchStatusReportsMismatchedResponseId()
{
    // WHAT: Verify that the local client rejects responses whose request ID does not match.
    // HOW: Serve a syntactically valid response from a raw local server but change the echoed
    // request ID, then confirm that the client reports a mismatched-response error.
    // WHY: Request IDs are the only protection against correlating the wrong response to a
    // request, so this check is critical for reliable control-plane behavior.
    const QString socketName = uniqueSocketName();
    QLocalServer server;
    QLocalServer::removeServer(socketName);
    QVERIFY(server.listen(socketName));
    const auto removeSocket = qScopeGuard([&socketName]() { QLocalServer::removeServer(socketName); });

    SessionFetchThread fetchThread(socketName, SessionFetchWorker::Operation::FetchStatus);
    fetchThread.start();

    QVERIFY(server.waitForNewConnection(1000));
    std::unique_ptr<QLocalSocket> socket(server.nextPendingConnection());
    QVERIFY(socket != nullptr);
    QVERIFY(socket->waitForReadyRead(1000));

    DaemonControlRequest request;
    QString parseError;
    QVERIFY(parseDaemonControlRequest(socket->readLine(), &request, &parseError));

    DaemonControlResponse response;
    response.requestId = QStringLiteral("different-id");
    response.success = true;
    response.result = daemonStatusSnapshotToJsonObject(DaemonStatusSnapshot{
        .daemonRunning = true,
        .configPath = QStringLiteral("/tmp/test.json"),
        .configExists = false,
        .serviceDiagnostics = QJsonObject{},
    });
    QVERIFY(socket->write(serializeDaemonControlResponse(response)) > 0);
    QVERIFY(socket->waitForBytesWritten(1000));

    fetchThread.wait();

    const DaemonStatusResult result = fetchThread.statusResult();

    QVERIFY(!result.success);
    QCOMPARE(result.errorMessage, QStringLiteral("Mismatched daemon response id"));
}

void DaemonControlClientServerTest::fetchStatusReportsMalformedSnapshotPayload()
{
    // WHAT: Verify that the local client reports malformed status payloads from the server.
    // HOW: Serve a successful protocol response whose result object does not match the typed
    // daemon status schema, then confirm that the parsed client result contains that error.
    // WHY: Transport success is not enough on its own; the client must still reject broken
    // payloads so UI and tooling do not trust invalid daemon state.
    const QString socketName = uniqueSocketName();
    QLocalServer server;
    QLocalServer::removeServer(socketName);
    QVERIFY(server.listen(socketName));
    const auto removeSocket = qScopeGuard([&socketName]() { QLocalServer::removeServer(socketName); });

    SessionFetchThread fetchThread(socketName, SessionFetchWorker::Operation::FetchStatus);
    fetchThread.start();

    QVERIFY(server.waitForNewConnection(1000));
    std::unique_ptr<QLocalSocket> socket(server.nextPendingConnection());
    QVERIFY(socket != nullptr);
    QVERIFY(socket->waitForReadyRead(1000));

    DaemonControlRequest request;
    QString parseError;
    QVERIFY(parseDaemonControlRequest(socket->readLine(), &request, &parseError));

    DaemonControlResponse response;
    response.requestId = request.requestId;
    response.success = true;
    response.result.insert(QStringLiteral("config_path"), QStringLiteral("/tmp/test.json"));
    QVERIFY(socket->write(serializeDaemonControlResponse(response)) > 0);
    QVERIFY(socket->waitForBytesWritten(1000));

    fetchThread.wait();

    const DaemonStatusResult result = fetchThread.statusResult();

    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains(QStringLiteral("Malformed daemon status payload")));
}

QTEST_GUILESS_MAIN(DaemonControlClientServerTest)

#include "daemoncontrolclientservertest.moc"
