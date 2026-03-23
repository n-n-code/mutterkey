#include "config.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QTest>

class ConfigTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadConfigUsesDefaultsWhenFileIsMissing();
    void loadConfigAppliesJsonOverrides();
    void loadConfigRejectsInvalidValues();
    void loadConfigReportsMalformedJson();
    void loadConfigIgnoresWrongJsonTypes();
    void loadConfigTrimsImportantStringFields();
};

void ConfigTest::loadConfigUsesDefaultsWhenFileIsMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString errorMessage;
    const AppConfig config = loadConfig(tempDir.filePath(QStringLiteral("missing.json")), &errorMessage);

    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(config.shortcut.sequence, QStringLiteral("F8"));
    QCOMPARE(config.audio.sampleRate, 16000);
    QCOMPARE(config.audio.channels, 1);
    QCOMPARE(config.audio.minimumSeconds, 0.25);
    QCOMPARE(config.transcriber.modelPath, defaultModelPath());
    QCOMPARE(config.transcriber.language, QStringLiteral("en"));
    QCOMPARE(config.transcriber.translate, false);
    QCOMPARE(config.transcriber.threads, 0);
    QCOMPARE(config.transcriber.warmupOnStart, false);
    QCOMPARE(config.logLevel, QStringLiteral("INFO"));
}

void ConfigTest::loadConfigAppliesJsonOverrides()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("config.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"json({
  "shortcut": {
    "component_unique": "mutterkey.test",
    "component_friendly": "Mutterkey Test",
    "action_unique": "ptt_test",
    "action_friendly": "Push To Talk Test",
    "sequence": "Meta+F8"
  },
  "audio": {
    "sample_rate": 48000,
    "channels": 2,
    "minimum_seconds": 0.75,
    "device_id": "test-mic"
  },
  "transcriber": {
    "model_path": "/tmp/test-model.bin",
    "language": "fi",
    "translate": true,
    "threads": 4,
    "warmup_on_start": true
  },
  "log_level": "debug"
})json");
    file.close();

    QString errorMessage;
    const AppConfig config = loadConfig(configPath, &errorMessage);

    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(config.shortcut.componentUnique, QStringLiteral("mutterkey.test"));
    QCOMPARE(config.shortcut.componentFriendly, QStringLiteral("Mutterkey Test"));
    QCOMPARE(config.shortcut.actionUnique, QStringLiteral("ptt_test"));
    QCOMPARE(config.shortcut.actionFriendly, QStringLiteral("Push To Talk Test"));
    QCOMPARE(config.shortcut.sequence, QStringLiteral("Meta+F8"));
    QCOMPARE(config.audio.sampleRate, 48000);
    QCOMPARE(config.audio.channels, 2);
    QCOMPARE(config.audio.minimumSeconds, 0.75);
    QCOMPARE(config.audio.deviceId, QStringLiteral("test-mic"));
    QCOMPARE(config.transcriber.modelPath, QStringLiteral("/tmp/test-model.bin"));
    QCOMPARE(config.transcriber.language, QStringLiteral("fi"));
    QCOMPARE(config.transcriber.translate, true);
    QCOMPARE(config.transcriber.threads, 4);
    QCOMPARE(config.transcriber.warmupOnStart, true);
    QCOMPARE(config.logLevel, QStringLiteral("DEBUG"));
}

void ConfigTest::loadConfigRejectsInvalidValues()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("config.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"json({
  "shortcut": {
    "sequence": "   "
  },
  "audio": {
    "sample_rate": 0,
    "channels": 0,
    "minimum_seconds": -1.0
  },
  "transcriber": {
    "model_path": "   ",
    "threads": -4
  },
  "log_level": "verbose"
})json");
    file.close();

    QString errorMessage;
    const AppConfig config = loadConfig(configPath, &errorMessage);

    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(config.shortcut.sequence, QStringLiteral("F8"));
    QCOMPARE(config.audio.sampleRate, 16000);
    QCOMPARE(config.audio.channels, 1);
    QCOMPARE(config.audio.minimumSeconds, 0.25);
    QCOMPARE(config.transcriber.modelPath, defaultModelPath());
    QCOMPARE(config.transcriber.threads, 0);
    QCOMPARE(config.logLevel, QStringLiteral("INFO"));
}

void ConfigTest::loadConfigReportsMalformedJson()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("config.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"json({
  "shortcut": {
    "sequence": "F9"
  }
)json");
    file.close();

    QString errorMessage;
    const AppConfig config = loadConfig(configPath, &errorMessage);

    QVERIFY(!errorMessage.isEmpty());
    QVERIFY(errorMessage.contains(QStringLiteral("Invalid JSON config")));
    QCOMPARE(config.shortcut.sequence, QStringLiteral("F8"));
    QCOMPARE(config.transcriber.modelPath, defaultModelPath());
}

void ConfigTest::loadConfigIgnoresWrongJsonTypes()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("config.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"json({
  "shortcut": {
    "sequence": 42
  },
  "audio": {
    "sample_rate": "16000",
    "channels": true,
    "minimum_seconds": "0.5",
    "device_id": 123
  },
  "transcriber": {
    "language": false,
    "translate": "yes",
    "threads": "8",
    "warmup_on_start": 1
  },
  "log_level": false
})json");
    file.close();

    QString errorMessage;
    const AppConfig config = loadConfig(configPath, &errorMessage);

    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(config.shortcut.sequence, QStringLiteral("F8"));
    QCOMPARE(config.audio.sampleRate, 16000);
    QCOMPARE(config.audio.channels, 1);
    QCOMPARE(config.audio.minimumSeconds, 0.25);
    QCOMPARE(config.audio.deviceId, QString());
    QCOMPARE(config.transcriber.language, QStringLiteral("en"));
    QCOMPARE(config.transcriber.translate, false);
    QCOMPARE(config.transcriber.threads, 0);
    QCOMPARE(config.transcriber.warmupOnStart, false);
    QCOMPARE(config.logLevel, QStringLiteral("INFO"));
}

void ConfigTest::loadConfigTrimsImportantStringFields()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("config.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"json({
  "shortcut": {
    "sequence": "  Meta+F8  "
  },
  "transcriber": {
    "model_path": "  /tmp/test-model.bin  "
  },
  "log_level": " warning "
})json");
    file.close();

    QString errorMessage;
    const AppConfig config = loadConfig(configPath, &errorMessage);

    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(config.shortcut.sequence, QStringLiteral("Meta+F8"));
    QCOMPARE(config.transcriber.modelPath, QStringLiteral("/tmp/test-model.bin"));
    QCOMPARE(config.logLevel, QStringLiteral("WARNING"));
}

QTEST_APPLESS_MAIN(ConfigTest)

#include "configtest.moc"
