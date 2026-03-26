#include "control/daemoncontroltypes.h"

#include <QtTest/QTest>

namespace {

class DaemonControlTypesTest final : public QObject
{
    Q_OBJECT

private slots:
    void parseStatusSnapshot();
    void parseConfigSnapshot();
    void rejectMalformedStatusSnapshot();
};

} // namespace

void DaemonControlTypesTest::parseStatusSnapshot()
{
    // WHAT: Verify that a daemon status snapshot can be converted to JSON and back.
    // HOW: Build a populated status snapshot, serialize it to a JSON object, parse it
    // again, and compare the key fields with the original values.
    // WHY: Status snapshots are shown to operators and tray UI code, so stable typed
    // parsing is necessary for trustworthy status reporting.
    DaemonStatusSnapshot input;
    input.daemonRunning = true;
    input.configPath = QStringLiteral("/tmp/mutterkey.json");
    input.configExists = true;
    input.serviceDiagnostics.insert(QStringLiteral("transcriptions_completed"), 4);

    DaemonStatusSnapshot parsed;
    QString errorMessage;
    QVERIFY(parseDaemonStatusSnapshot(daemonStatusSnapshotToJsonObject(input), &parsed, &errorMessage));
    QVERIFY(parsed.daemonRunning);
    QCOMPARE(parsed.configPath, input.configPath);
    QCOMPARE(parsed.serviceDiagnostics.value(QStringLiteral("transcriptions_completed")).toInt(), 4);
}

void DaemonControlTypesTest::parseConfigSnapshot()
{
    // WHAT: Verify that a daemon config snapshot can be converted to JSON and back.
    // HOW: Build a config snapshot with representative values, serialize it, parse it,
    // and check that the parsed snapshot still contains the same configuration data.
    // WHY: This protects the contract used to inspect daemon configuration remotely, which
    // helps operators and UI surfaces present accurate settings.
    DaemonConfigSnapshot input;
    input.configPath = QStringLiteral("/tmp/mutterkey.json");
    input.configExists = true;
    input.config.shortcut.sequence = QStringLiteral("Meta+F8");
    input.config.transcriber.modelPath = QStringLiteral("/tmp/model.bin");

    DaemonConfigSnapshot parsed;
    QString errorMessage;
    QVERIFY(parseDaemonConfigSnapshot(daemonConfigSnapshotToJsonObject(input), &parsed, &errorMessage));
    QCOMPARE(parsed.configPath, input.configPath);
    QCOMPARE(parsed.config.shortcut.sequence, QStringLiteral("Meta+F8"));
    QCOMPARE(parsed.config.transcriber.modelPath, QStringLiteral("/tmp/model.bin"));
}

void DaemonControlTypesTest::rejectMalformedStatusSnapshot()
{
    // WHAT: Verify that malformed status payloads are rejected.
    // HOW: Attempt to parse a JSON object that is missing required status fields and check
    // that parsing fails with a malformed-payload error.
    // WHY: Rejecting incomplete status data prevents the rest of the application from
    // treating broken control-plane messages as trustworthy state.
    DaemonStatusSnapshot parsed;
    QString errorMessage;
    QVERIFY(!parseDaemonStatusSnapshot(QJsonObject{{QStringLiteral("config_path"), QStringLiteral("/tmp/x")}}, &parsed, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("Malformed daemon status payload")));
}

QTEST_APPLESS_MAIN(DaemonControlTypesTest)

#include "daemoncontroltypestest.moc"
