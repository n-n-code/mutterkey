#include "control/daemoncontrolclient.h"
#include "tray/traystatuswindow.h"

#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QtTest/QTest>

namespace {

template <typename T>
T &requireChild(const QObject &parent, const char *objectName)
{
    auto *child = parent.findChild<T *>(QLatin1StringView(objectName));
    if (child == nullptr) {
        qFatal("Missing child widget: %s", objectName);
    }

    return *child;
}

class FakeDaemonControlSession final : public DaemonControlSession
{
public:
    [[nodiscard]] DaemonStatusResult fetchStatus(int timeoutMs) const override
    {
        Q_UNUSED(timeoutMs)
        return m_statusResult;
    }

    [[nodiscard]] DaemonConfigResult fetchConfig(int timeoutMs) const override
    {
        Q_UNUSED(timeoutMs)
        return m_configResult;
    }

    void setStatusResult(DaemonStatusResult result) { m_statusResult = std::move(result); }
    void setConfigResult(DaemonConfigResult result) { m_configResult = std::move(result); }

private:
    DaemonStatusResult m_statusResult;
    DaemonConfigResult m_configResult;
};

class TrayStatusWindowTest final : public QObject
{
    Q_OBJECT

private slots:
    void refreshShowsOfflineStateWhenTransportFails();
    void refreshPopulatesValuesOnSuccessfulResponses();
    void refreshShowsOfflineStateWhenConfigRequestFails();
    void refreshUpdatesFromOfflineToConnected();
    void refreshUpdatesFromConnectedToOffline();
};

} // namespace

void TrayStatusWindowTest::refreshShowsOfflineStateWhenTransportFails()
{
    // WHAT: Verify that the tray status window shows an offline state when status transport fails.
    // HOW: Make the fake daemon session return a connection error, construct the window,
    // and confirm that the visible labels and JSON panel show the unavailable state.
    // WHY: When the daemon cannot be reached, the UI must communicate that clearly so users
    // do not mistake a transport problem for a healthy but idle daemon.
    FakeDaemonControlSession client;
    DaemonStatusResult statusResult;
    statusResult.errorMessage = QStringLiteral("Connection refused");
    client.setStatusResult(statusResult);

    const TrayStatusWindow window(&client);

    const QLabel &connectionValue = requireChild<QLabel>(window, "connectionValue");
    const QLabel &configPathValue = requireChild<QLabel>(window, "configPathValue");
    const QPlainTextEdit &statusJsonView = requireChild<QPlainTextEdit>(window, "statusJsonView");

    QCOMPARE(connectionValue.text(), QStringLiteral("Daemon unavailable"));
    QCOMPARE(configPathValue.text(), QStringLiteral("-"));
    QVERIFY(statusJsonView.toPlainText().contains(QStringLiteral("Connection refused")));
}

void TrayStatusWindowTest::refreshPopulatesValuesOnSuccessfulResponses()
{
    // WHAT: Verify that the tray status window shows live values after successful responses.
    // HOW: Return successful fake status and config snapshots, construct the window, and
    // check that the key labels and JSON view contain the expected values.
    // WHY: This is the main inspection surface for the tray UI, so it must faithfully show
    // what the daemon reports instead of stale or placeholder information.
    FakeDaemonControlSession client;

    DaemonStatusResult statusResult;
    statusResult.success = true;
    statusResult.snapshot.daemonRunning = true;
    statusResult.snapshot.configPath = QStringLiteral("/tmp/mutterkey.json");
    statusResult.snapshot.configExists = true;
    statusResult.snapshot.serviceDiagnostics.insert(QStringLiteral("daemon_running"), true);
    client.setStatusResult(statusResult);

    DaemonConfigResult configResult;
    configResult.success = true;
    configResult.snapshot.configPath = QStringLiteral("/tmp/mutterkey.json");
    configResult.snapshot.configExists = true;
    configResult.snapshot.config.shortcut.sequence = QStringLiteral("Meta+F8");
    configResult.snapshot.config.transcriber.modelPath = QStringLiteral("/tmp/model.bin");
    client.setConfigResult(configResult);

    const TrayStatusWindow window(&client);

    const QLabel &connectionValue = requireChild<QLabel>(window, "connectionValue");
    const QLabel &configPathValue = requireChild<QLabel>(window, "configPathValue");
    const QLabel &shortcutValue = requireChild<QLabel>(window, "shortcutValue");
    const QLabel &modelPathValue = requireChild<QLabel>(window, "modelPathValue");
    const QPlainTextEdit &statusJsonView = requireChild<QPlainTextEdit>(window, "statusJsonView");

    QCOMPARE(connectionValue.text(), QStringLiteral("Connected"));
    QCOMPARE(configPathValue.text(), QStringLiteral("/tmp/mutterkey.json"));
    QCOMPARE(shortcutValue.text(), QStringLiteral("Meta+F8"));
    QCOMPARE(modelPathValue.text(), QStringLiteral("/tmp/model.bin"));
    QVERIFY(statusJsonView.toPlainText().contains(QStringLiteral("daemon_running")));
}

void TrayStatusWindowTest::refreshShowsOfflineStateWhenConfigRequestFails()
{
    // WHAT: Verify that the tray status window falls back to an offline state when config loading fails.
    // HOW: Return a successful status fetch but a failed config fetch, then confirm that the
    // window marks the daemon as unavailable and exposes the config error to the user.
    // WHY: A partial control-plane failure still leaves the UI without trustworthy state, so
    // the window should prefer an explicit problem state over pretending everything is healthy.
    FakeDaemonControlSession client;

    DaemonStatusResult statusResult;
    statusResult.success = true;
    statusResult.snapshot.configPath = QStringLiteral("/tmp/mutterkey.json");
    client.setStatusResult(statusResult);

    DaemonConfigResult configResult;
    configResult.errorMessage = QStringLiteral("Config unavailable");
    client.setConfigResult(configResult);

    const TrayStatusWindow window(&client);

    const QLabel &connectionValue = requireChild<QLabel>(window, "connectionValue");
    const QPlainTextEdit &statusJsonView = requireChild<QPlainTextEdit>(window, "statusJsonView");

    QCOMPARE(connectionValue.text(), QStringLiteral("Daemon unavailable"));
    QVERIFY(statusJsonView.toPlainText().contains(QStringLiteral("Config unavailable")));
}

void TrayStatusWindowTest::refreshUpdatesFromOfflineToConnected()
{
    // WHAT: Verify that a manual refresh can move the tray status window from offline to connected.
    // HOW: Construct the window with an initial transport failure, then swap the fake session
    // to successful status and config responses, call refresh, and check the visible fields.
    // WHY: The tray window is a live status surface, so it must recover cleanly after the
    // daemon becomes available instead of staying stuck in its initial error state.
    FakeDaemonControlSession client;
    client.setStatusResult(DaemonStatusResult{.success = false, .snapshot = {}, .errorMessage = QStringLiteral("Connection refused")});

    TrayStatusWindow window(&client);

    DaemonStatusResult statusResult;
    statusResult.success = true;
    statusResult.snapshot.daemonRunning = true;
    statusResult.snapshot.configPath = QStringLiteral("/tmp/mutterkey.json");
    statusResult.snapshot.configExists = true;
    statusResult.snapshot.serviceDiagnostics.insert(QStringLiteral("daemon_running"), true);
    client.setStatusResult(statusResult);

    DaemonConfigResult configResult;
    configResult.success = true;
    configResult.snapshot.configPath = QStringLiteral("/tmp/mutterkey.json");
    configResult.snapshot.configExists = true;
    configResult.snapshot.config.shortcut.sequence = QStringLiteral("Meta+F8");
    configResult.snapshot.config.transcriber.modelPath = QStringLiteral("/tmp/model.bin");
    client.setConfigResult(configResult);

    window.refresh();

    const QLabel &connectionValue = requireChild<QLabel>(window, "connectionValue");
    const QLabel &configPathValue = requireChild<QLabel>(window, "configPathValue");
    const QPlainTextEdit &statusJsonView = requireChild<QPlainTextEdit>(window, "statusJsonView");

    QCOMPARE(connectionValue.text(), QStringLiteral("Connected"));
    QCOMPARE(configPathValue.text(), QStringLiteral("/tmp/mutterkey.json"));
    QVERIFY(statusJsonView.toPlainText().contains(QStringLiteral("daemon_running")));
}

void TrayStatusWindowTest::refreshUpdatesFromConnectedToOffline()
{
    // WHAT: Verify that a manual refresh can move the tray status window from connected to offline.
    // HOW: Construct the window with successful status and config responses, then switch the
    // fake session to a failure response, call refresh, and confirm that the offline state wins.
    // WHY: Live status views must degrade cleanly when the daemon disappears so users are not
    // left looking at stale data that appears current.
    FakeDaemonControlSession client;

    DaemonStatusResult statusResult;
    statusResult.success = true;
    statusResult.snapshot.daemonRunning = true;
    statusResult.snapshot.configPath = QStringLiteral("/tmp/mutterkey.json");
    statusResult.snapshot.configExists = true;
    statusResult.snapshot.serviceDiagnostics.insert(QStringLiteral("daemon_running"), true);
    client.setStatusResult(statusResult);

    DaemonConfigResult configResult;
    configResult.success = true;
    configResult.snapshot.configPath = QStringLiteral("/tmp/mutterkey.json");
    configResult.snapshot.configExists = true;
    configResult.snapshot.config.shortcut.sequence = QStringLiteral("Meta+F8");
    configResult.snapshot.config.transcriber.modelPath = QStringLiteral("/tmp/model.bin");
    client.setConfigResult(configResult);

    TrayStatusWindow window(&client);

    client.setStatusResult(DaemonStatusResult{.success = false, .snapshot = {}, .errorMessage = QStringLiteral("Connection refused")});
    window.refresh();

    const QLabel &connectionValue = requireChild<QLabel>(window, "connectionValue");
    const QLabel &configPathValue = requireChild<QLabel>(window, "configPathValue");
    const QPlainTextEdit &statusJsonView = requireChild<QPlainTextEdit>(window, "statusJsonView");

    QCOMPARE(connectionValue.text(), QStringLiteral("Daemon unavailable"));
    QCOMPARE(configPathValue.text(), QStringLiteral("-"));
    QVERIFY(statusJsonView.toPlainText().contains(QStringLiteral("Connection refused")));
}

QTEST_MAIN(TrayStatusWindowTest)

#include "traystatuswindowtest.moc"
