#include "config.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QTest>

class ConfigTest final : public QObject
{
    Q_OBJECT

private slots:
    void defaultAppConfigMatchesDocumentedDefaults();
    void loadConfigUsesDefaultsWhenFileIsMissing();
    void loadConfigAppliesJsonOverrides();
    void loadConfigRejectsInvalidValues();
    void loadConfigReportsMalformedJson();
    void loadConfigIgnoresWrongJsonTypes();
    void loadConfigTrimsImportantStringFields();
    void saveConfigRoundTripsResolvedValues();
    void saveConfigCreatesParentDirectory();
    void applyConfigValueUpdatesSupportedFields();
    void applyConfigValueRejectsInvalidInputs();
    void applyConfigValueRejectsUnknownKeys();
};

void ConfigTest::defaultAppConfigMatchesDocumentedDefaults()
{
    const AppConfig config = defaultAppConfig();

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
    "language": "pirate",
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
    QCOMPARE(config.transcriber.language, QStringLiteral("en"));
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
    "model_path": "  /tmp/test-model.bin  ",
    "language": "  AUTO  "
  },
  "log_level": " warning "
})json");
    file.close();

    QString errorMessage;
    const AppConfig config = loadConfig(configPath, &errorMessage);

    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(config.shortcut.sequence, QStringLiteral("Meta+F8"));
    QCOMPARE(config.transcriber.modelPath, QStringLiteral("/tmp/test-model.bin"));
    QCOMPARE(config.transcriber.language, QStringLiteral("auto"));
    QCOMPARE(config.logLevel, QStringLiteral("WARNING"));
}

void ConfigTest::saveConfigRoundTripsResolvedValues()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    AppConfig config = defaultAppConfig();
    config.shortcut.sequence = QStringLiteral("Meta+F8");
    config.audio.sampleRate = 48000;
    config.audio.channels = 2;
    config.audio.minimumSeconds = 0.5;
    config.audio.deviceId = QStringLiteral("usb-mic");
    config.transcriber.modelPath = QStringLiteral("/tmp/test-model.bin");
    config.transcriber.language = QStringLiteral("fi");
    config.transcriber.translate = true;
    config.transcriber.threads = 6;
    config.transcriber.warmupOnStart = true;
    config.logLevel = QStringLiteral("DEBUG");

    const QString configPath = tempDir.filePath(QStringLiteral("config.json"));
    QString errorMessage;
    QVERIFY(saveConfig(configPath, config, &errorMessage));
    QVERIFY(errorMessage.isEmpty());

    const AppConfig loadedConfig = loadConfig(configPath, &errorMessage);
    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(loadedConfig.shortcut.sequence, config.shortcut.sequence);
    QCOMPARE(loadedConfig.audio.sampleRate, config.audio.sampleRate);
    QCOMPARE(loadedConfig.audio.channels, config.audio.channels);
    QCOMPARE(loadedConfig.audio.minimumSeconds, config.audio.minimumSeconds);
    QCOMPARE(loadedConfig.audio.deviceId, config.audio.deviceId);
    QCOMPARE(loadedConfig.transcriber.modelPath, config.transcriber.modelPath);
    QCOMPARE(loadedConfig.transcriber.language, config.transcriber.language);
    QCOMPARE(loadedConfig.transcriber.translate, config.transcriber.translate);
    QCOMPARE(loadedConfig.transcriber.threads, config.transcriber.threads);
    QCOMPARE(loadedConfig.transcriber.warmupOnStart, config.transcriber.warmupOnStart);
    QCOMPARE(loadedConfig.logLevel, config.logLevel);
}

void ConfigTest::saveConfigCreatesParentDirectory()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("nested/mutterkey/config.json"));
    QString errorMessage;
    QVERIFY(saveConfig(configPath, defaultAppConfig(), &errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QVERIFY(QFile::exists(configPath));
}

void ConfigTest::applyConfigValueUpdatesSupportedFields()
{
    AppConfig config = defaultAppConfig();
    QString errorMessage;

    QVERIFY(applyConfigValue(&config, QStringLiteral("shortcut.sequence"), QStringLiteral("Meta+F8"), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("audio.sample_rate"), QStringLiteral("48000"), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("audio.channels"), QStringLiteral("2"), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("audio.minimum_seconds"), QStringLiteral("0.75"), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("audio.device_id"), QStringLiteral("  test-mic  "), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("transcriber.model_path"), QStringLiteral(" /tmp/model.bin "), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("transcriber.language"), QStringLiteral(" fi "), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("transcriber.language"), QStringLiteral(" auto "), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("transcriber.translate"), QStringLiteral("yes"), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("transcriber.threads"), QStringLiteral("3"), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("transcriber.warmup_on_start"), QStringLiteral("on"), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("log_level"), QStringLiteral(" warning "), &errorMessage));
    QVERIFY(errorMessage.isEmpty());

    QCOMPARE(config.shortcut.sequence, QStringLiteral("Meta+F8"));
    QCOMPARE(config.audio.sampleRate, 48000);
    QCOMPARE(config.audio.channels, 2);
    QCOMPARE(config.audio.minimumSeconds, 0.75);
    QCOMPARE(config.audio.deviceId, QStringLiteral("test-mic"));
    QCOMPARE(config.transcriber.modelPath, QStringLiteral("/tmp/model.bin"));
    QCOMPARE(config.transcriber.language, QStringLiteral("auto"));
    QCOMPARE(config.transcriber.translate, true);
    QCOMPARE(config.transcriber.threads, 3);
    QCOMPARE(config.transcriber.warmupOnStart, true);
    QCOMPARE(config.logLevel, QStringLiteral("WARNING"));
}

void ConfigTest::applyConfigValueRejectsInvalidInputs()
{
    const AppConfig originalConfig = defaultAppConfig();
    AppConfig config = originalConfig;
    QString errorMessage;

    QVERIFY(!applyConfigValue(&config, QStringLiteral("transcriber.model_path"), QStringLiteral("   "), &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("may not be empty")));
    QCOMPARE(config.transcriber.modelPath, originalConfig.transcriber.modelPath);

    errorMessage.clear();
    QVERIFY(!applyConfigValue(&config, QStringLiteral("transcriber.threads"), QStringLiteral("-1"), &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("0 or greater")));
    QCOMPARE(config.transcriber.threads, originalConfig.transcriber.threads);

    errorMessage.clear();
    QVERIFY(!applyConfigValue(&config, QStringLiteral("transcriber.translate"), QStringLiteral("maybe"), &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("boolean")));
    QCOMPARE(config.transcriber.translate, originalConfig.transcriber.translate);

    errorMessage.clear();
    QVERIFY(!applyConfigValue(&config, QStringLiteral("transcriber.language"), QStringLiteral("pirate"), &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("supported Whisper language")));
    QCOMPARE(config.transcriber.language, originalConfig.transcriber.language);

    errorMessage.clear();
    QVERIFY(!applyConfigValue(&config, QStringLiteral("log_level"), QStringLiteral("verbose"), &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("DEBUG")));
    QCOMPARE(config.logLevel, originalConfig.logLevel);
}

void ConfigTest::applyConfigValueRejectsUnknownKeys()
{
    AppConfig config = defaultAppConfig();
    QString errorMessage;

    QVERIFY(!applyConfigValue(&config, QStringLiteral("shortcut.unknown"), QStringLiteral("F9"), &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("Unsupported config key")));
}

QTEST_APPLESS_MAIN(ConfigTest)

#include "configtest.moc"
