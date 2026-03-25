#include "control/daemoncontroltypes.h"

#include <QtTest/QTest>

class DaemonControlTypesTest final : public QObject
{
    Q_OBJECT

private slots:
    void parseStatusSnapshot();
    void parseConfigSnapshot();
    void rejectMalformedStatusSnapshot();
};

void DaemonControlTypesTest::parseStatusSnapshot()
{
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
    DaemonStatusSnapshot parsed;
    QString errorMessage;
    QVERIFY(!parseDaemonStatusSnapshot(QJsonObject{{QStringLiteral("config_path"), QStringLiteral("/tmp/x")}}, &parsed, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("Malformed daemon status payload")));
}

QTEST_APPLESS_MAIN(DaemonControlTypesTest)

#include "daemoncontroltypestest.moc"
