#include "control/daemoncontrolclient.h"
#include "tray/traystatuswindow.h"

#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QtTest/QTest>

namespace {

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
};

} // namespace

void TrayStatusWindowTest::refreshShowsOfflineStateWhenTransportFails()
{
    FakeDaemonControlSession client;
    DaemonStatusResult statusResult;
    statusResult.errorMessage = QStringLiteral("Connection refused");
    client.setStatusResult(statusResult);

    const TrayStatusWindow window(&client);

    auto *connectionValue = window.findChild<QLabel *>(QStringLiteral("connectionValue"));
    auto *configPathValue = window.findChild<QLabel *>(QStringLiteral("configPathValue"));
    auto *statusJsonView = window.findChild<QPlainTextEdit *>(QStringLiteral("statusJsonView"));

    QVERIFY(connectionValue != nullptr);
    QVERIFY(configPathValue != nullptr);
    QVERIFY(statusJsonView != nullptr);

    QCOMPARE(connectionValue->text(), QStringLiteral("Daemon unavailable"));
    QCOMPARE(configPathValue->text(), QStringLiteral("-"));
    QVERIFY(statusJsonView->toPlainText().contains(QStringLiteral("Connection refused")));
}

void TrayStatusWindowTest::refreshPopulatesValuesOnSuccessfulResponses()
{
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

    auto *connectionValue = window.findChild<QLabel *>(QStringLiteral("connectionValue"));
    auto *configPathValue = window.findChild<QLabel *>(QStringLiteral("configPathValue"));
    auto *shortcutValue = window.findChild<QLabel *>(QStringLiteral("shortcutValue"));
    auto *modelPathValue = window.findChild<QLabel *>(QStringLiteral("modelPathValue"));
    auto *statusJsonView = window.findChild<QPlainTextEdit *>(QStringLiteral("statusJsonView"));

    QVERIFY(connectionValue != nullptr);
    QVERIFY(configPathValue != nullptr);
    QVERIFY(shortcutValue != nullptr);
    QVERIFY(modelPathValue != nullptr);
    QVERIFY(statusJsonView != nullptr);

    QCOMPARE(connectionValue->text(), QStringLiteral("Connected"));
    QCOMPARE(configPathValue->text(), QStringLiteral("/tmp/mutterkey.json"));
    QCOMPARE(shortcutValue->text(), QStringLiteral("Meta+F8"));
    QCOMPARE(modelPathValue->text(), QStringLiteral("/tmp/model.bin"));
    QVERIFY(statusJsonView->toPlainText().contains(QStringLiteral("daemon_running")));
}

void TrayStatusWindowTest::refreshShowsOfflineStateWhenConfigRequestFails()
{
    FakeDaemonControlSession client;

    DaemonStatusResult statusResult;
    statusResult.success = true;
    statusResult.snapshot.configPath = QStringLiteral("/tmp/mutterkey.json");
    client.setStatusResult(statusResult);

    DaemonConfigResult configResult;
    configResult.errorMessage = QStringLiteral("Config unavailable");
    client.setConfigResult(configResult);

    const TrayStatusWindow window(&client);

    auto *connectionValue = window.findChild<QLabel *>(QStringLiteral("connectionValue"));
    auto *statusJsonView = window.findChild<QPlainTextEdit *>(QStringLiteral("statusJsonView"));

    QVERIFY(connectionValue != nullptr);
    QVERIFY(statusJsonView != nullptr);

    QCOMPARE(connectionValue->text(), QStringLiteral("Daemon unavailable"));
    QVERIFY(statusJsonView->toPlainText().contains(QStringLiteral("Config unavailable")));
}

QTEST_MAIN(TrayStatusWindowTest)

#include "traystatuswindowtest.moc"
