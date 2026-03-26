#include "config.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QTest>

namespace {

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
    void configJsonObjectRoundTripsResolvedValues();
    void saveConfigCreatesParentDirectory();
    void applyConfigValueUpdatesSupportedFields();
    void applyConfigValueRejectsInvalidInputs();
    void applyConfigValueRejectsUnknownKeys();
    void applyConfigValueAcceptsBooleanFalseSpellings_data();
    void applyConfigValueAcceptsBooleanFalseSpellings();
};

} // namespace

void ConfigTest::defaultAppConfigMatchesDocumentedDefaults()
{
    // WHAT: Verify that the built-in default configuration matches the documented defaults.
    // HOW: Create the default config in memory and compare each important field against the
    // expected values described by the product's default behavior.
    // WHY: These defaults define how Mutterkey behaves before a user customizes anything,
    // so silent drift here would make documentation and first-run behavior disagree.
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
    // WHAT: Verify that loading a missing config file falls back to safe defaults.
    // HOW: Ask the loader to read a path that does not exist and confirm that it returns
    // the default values without reporting a hard error.
    // WHY: A missing file is a normal first-run situation, and the app must stay usable
    // instead of failing just because configuration has not been created yet.
    const QTemporaryDir tempDir;
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
    // WHAT: Verify that valid JSON config values override the built-in defaults.
    // HOW: Write a complete config file with custom values, load it, and compare the
    // resulting in-memory config against those custom values.
    // WHY: User configuration is only meaningful if saved values reliably take effect,
    // so this test protects the main customization path.
    const QTemporaryDir tempDir;
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
    // WHAT: Verify that invalid config values are rejected and replaced with defaults.
    // HOW: Load a config file that contains empty or out-of-range values,
    // then confirm that the resolved config falls back to the safe default values.
    // WHY: Configuration files can be edited by hand or become corrupted, and the app must
    // recover predictably instead of carrying invalid runtime settings forward.
    const QTemporaryDir tempDir;
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
    "channels": 9,
    "minimum_seconds": -1.0
  },
  "transcriber": {
    "model_path": "   ",
    "language": "   ",
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

void ConfigTest::configJsonObjectRoundTripsResolvedValues()
{
    // WHAT: Verify that the in-memory config JSON conversion round-trips resolved values.
    // HOW: Build a config with custom values, convert it to a JSON object, load that object
    // back through the shared parser, and compare the resulting config fields.
    // WHY: The daemon control plane uses in-memory config JSON objects rather than files, so
    // this test protects that contract directly instead of only the file-based path.
    AppConfig config = defaultAppConfig();
    config.shortcut.sequence = QStringLiteral("Meta+F8");
    config.audio.sampleRate = 48000;
    config.audio.channels = 2;
    config.audio.minimumSeconds = 0.0;
    config.audio.deviceId = QStringLiteral("usb-mic");
    config.transcriber.modelPath = QStringLiteral("/tmp/test-model.bin");
    config.transcriber.language = QStringLiteral("fi");
    config.transcriber.translate = true;
    config.transcriber.threads = 6;
    config.transcriber.warmupOnStart = true;
    config.logLevel = QStringLiteral("DEBUG");

    const AppConfig loadedConfig = loadConfigObject(configToJsonObject(config), QStringLiteral("test round trip"));

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

void ConfigTest::loadConfigReportsMalformedJson()
{
    // WHAT: Verify that malformed JSON is reported as an error.
    // HOW: Save a broken JSON document, load it, and assert that an explanatory error
    // message is returned while the resolved config falls back to defaults.
    // WHY: When the file itself is syntactically broken, users need a clear signal that
    // the problem is the file format and not an unrelated runtime failure.
    const QTemporaryDir tempDir;
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
    // WHAT: Verify that values with the wrong JSON type are ignored.
    // HOW: Provide numbers where strings are expected, strings where booleans are expected,
    // and similar mismatches, then confirm that defaults remain in place.
    // WHY: This keeps config loading robust when users or tools produce structurally valid
    // JSON that still does not match the schema Mutterkey expects.
    const QTemporaryDir tempDir;
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
    // WHAT: Verify that important string settings are trimmed and normalized on load.
    // HOW: Load values padded with extra whitespace and mixed casing, then check that the
    // resolved config stores the cleaned, normalized forms.
    // WHY: Small formatting mistakes in a hand-edited config should not break the app or
    // force users to debug invisible whitespace problems.
    const QTemporaryDir tempDir;
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
    // WHAT: Verify that saving and then reloading a config preserves the resolved values.
    // HOW: Build a config with custom values, save it to disk, load it back, and compare
    // the loaded fields with the original in-memory values.
    // WHY: This protects the persistence path so that a saved configuration comes back the
    // same way on the next run instead of silently changing user settings.
    const QTemporaryDir tempDir;
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
    // WHAT: Verify that saving a config creates missing parent directories.
    // HOW: Save to a nested path whose directories do not yet exist and then confirm that
    // the file was created successfully.
    // WHY: Users should not need to manually prepare directory trees before using config
    // commands, especially during first-time setup.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("nested/mutterkey/config.json"));
    QString errorMessage;
    QVERIFY(saveConfig(configPath, defaultAppConfig(), &errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QVERIFY(QFile::exists(configPath));
}

void ConfigTest::applyConfigValueUpdatesSupportedFields()
{
    // WHAT: Verify that supported config keys accept valid updates from string input.
    // HOW: Apply a series of key/value updates through the CLI-style helper and confirm
    // that each target field changes to the expected normalized value.
    // WHY: The interactive and command-line config editing flows depend on this helper to
    // turn user input into stored settings safely and consistently.
    AppConfig config = defaultAppConfig();
    QString errorMessage;

    QVERIFY(applyConfigValue(&config, QStringLiteral("shortcut.sequence"), QStringLiteral("Meta+F8"), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("audio.sample_rate"), QStringLiteral("48000"), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("audio.channels"), QStringLiteral("2"), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("audio.minimum_seconds"), QStringLiteral("0"), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("audio.device_id"), QStringLiteral("  test-mic  "), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("transcriber.model_path"), QStringLiteral(" /tmp/model.bin "), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("transcriber.language"), QStringLiteral(" fi "), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("transcriber.language"), QStringLiteral(" Finnish "), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("transcriber.language"), QStringLiteral(" auto "), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("transcriber.translate"), QStringLiteral("yes"), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("transcriber.threads"), QStringLiteral("3"), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("transcriber.warmup_on_start"), QStringLiteral("on"), &errorMessage));
    QVERIFY(applyConfigValue(&config, QStringLiteral("log_level"), QStringLiteral(" warning "), &errorMessage));
    QVERIFY(errorMessage.isEmpty());

    QCOMPARE(config.shortcut.sequence, QStringLiteral("Meta+F8"));
    QCOMPARE(config.audio.sampleRate, 48000);
    QCOMPARE(config.audio.channels, 2);
    QCOMPARE(config.audio.minimumSeconds, 0.0);
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
    // WHAT: Verify that invalid config updates are rejected without changing the config.
    // HOW: Try empty, unsupported, or out-of-range values and confirm that the helper
    // returns an error while the previous valid values remain untouched.
    // WHY: A failed update should be safe to attempt, because partial or destructive config
    // writes would make CLI editing unreliable and hard to trust.
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
    QVERIFY(!applyConfigValue(&config, QStringLiteral("transcriber.language"), QStringLiteral("   "), &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("may not be empty")));
    QCOMPARE(config.transcriber.language, originalConfig.transcriber.language);

    errorMessage.clear();
    QVERIFY(!applyConfigValue(&config, QStringLiteral("log_level"), QStringLiteral("verbose"), &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("DEBUG")));
    QCOMPARE(config.logLevel, originalConfig.logLevel);
}

void ConfigTest::applyConfigValueRejectsUnknownKeys()
{
    // WHAT: Verify that unknown config keys are rejected.
    // HOW: Attempt to update a key that the application does not support and check that
    // the helper returns an "Unsupported config key" style error.
    // WHY: Rejecting unknown keys prevents silent no-op behavior and makes typos visible
    // to users immediately.
    AppConfig config = defaultAppConfig();
    QString errorMessage;

    QVERIFY(!applyConfigValue(&config, QStringLiteral("shortcut.unknown"), QStringLiteral("F9"), &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("Unsupported config key")));
}

void ConfigTest::applyConfigValueAcceptsBooleanFalseSpellings_data()
{
    QTest::addColumn<QString>("value");

    QTest::newRow("false") << QStringLiteral("false");
    QTest::newRow("zero") << QStringLiteral("0");
    QTest::newRow("no") << QStringLiteral("no");
    QTest::newRow("off") << QStringLiteral("off");
}

void ConfigTest::applyConfigValueAcceptsBooleanFalseSpellings()
{
    // WHAT: Verify that supported false-like boolean spellings are accepted for config updates.
    // HOW: Apply the same boolean field using a table of accepted false spellings and confirm
    // that each variant resolves to `false`.
    // WHY: CLI configuration should be forgiving about common boolean input styles so users
    // do not have to remember one exact textual representation.
    // NOLINTNEXTLINE(misc-const-correctness): QFETCH declares a mutable local by macro design.
    QFETCH(QString, value);

    AppConfig config = defaultAppConfig();
    config.transcriber.translate = true;
    QString errorMessage;

    QVERIFY(applyConfigValue(&config, QStringLiteral("transcriber.translate"), value, &errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(config.transcriber.translate, false);
}

QTEST_APPLESS_MAIN(ConfigTest)

#include "configtest.moc"
