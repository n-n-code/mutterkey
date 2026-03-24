#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

/**
 * @file
 * @brief Runtime configuration types, defaults, and JSON loading entrypoints.
 */

/**
 * @brief Returns the default runtime config path.
 *
 * Runtime defaults are intentionally centralized here so the loader, tests, and
 * any future CLI or config UI work from the same baseline values.
 *
 * @return Default path to `config.json`.
 */
QString defaultConfigPath();

/**
 * @brief Returns the default Whisper model path used for fallback config values.
 * @return Default path to the expected local model file.
 */
QString defaultModelPath();

/**
 * @brief Global shortcut registration settings.
 */
struct ShortcutConfig {
    /// Stable KDE component identifier used with KGlobalAccel.
    QString componentUnique = QStringLiteral("mutterkey");
    /// Human-readable component name shown by KDE tooling.
    QString componentFriendly = QStringLiteral("Mutterkey");
    /// Stable action identifier used for the push-to-talk action.
    QString actionUnique = QStringLiteral("push_to_talk");
    /// Human-readable action name shown by KDE tooling.
    QString actionFriendly = QStringLiteral("Push to Talk");
    /// Shortcut sequence accepted by QKeySequence parsing.
    QString sequence = QStringLiteral("F8");
};

/**
 * @brief Audio capture configuration passed to the recorder.
 */
struct AudioConfig {
    /// Preferred sample rate requested from the capture device.
    int sampleRate = 16000;
    /// Preferred channel count requested from the capture device.
    int channels = 1;
    /// Minimum recording duration required before transcription is attempted.
    double minimumSeconds = 0.25;
    /// Optional Qt multimedia device identifier. Empty selects the default input.
    QString deviceId;
};

/**
 * @brief Whisper backend configuration.
 */
struct TranscriberConfig {
    /// Filesystem path to the Whisper model file.
    QString modelPath;
    /// Language code passed to whisper.cpp.
    QString language = QStringLiteral("en");
    /// When `true`, translate speech to English instead of transcribing as-is.
    bool translate = false;
    /// Number of worker threads. `0` means auto-detect.
    int threads = 0;
    /// When `true`, initialize the backend during service startup.
    bool warmupOnStart = false;
};

/**
 * @brief Top-level runtime configuration snapshot.
 */
struct AppConfig {
    /// Global shortcut settings.
    ShortcutConfig shortcut;
    /// Audio capture settings.
    AudioConfig audio;
    /// Transcription backend settings.
    TranscriberConfig transcriber;
    /// Qt logging threshold accepted by the runtime logger setup.
    QString logLevel = QStringLiteral("INFO");
};

/**
 * @brief Returns the built-in runtime defaults.
 * @return Application config initialized with repo-defined defaults.
 */
AppConfig defaultAppConfig();

/**
 * @brief Loads a config file and applies repo-defined fallback defaults.
 *
 * Invalid or missing values are handled permissively where possible so runtime
 * startup can continue with safe defaults and warnings.
 *
 * @param path Path to the JSON config file.
 * @param errorMessage Optional output for fatal load or parse failures.
 * @return Resolved application config snapshot.
 */
AppConfig loadConfig(const QString &path, QString *errorMessage = nullptr);

/**
 * @brief Serializes a config snapshot to indented JSON.
 * @param config Config snapshot to serialize.
 * @return UTF-8 JSON payload.
 */
QByteArray serializeConfig(const AppConfig &config);

/**
 * @brief Saves a config snapshot to disk, creating parent directories as needed.
 * @param path Destination config path.
 * @param config Config snapshot to save.
 * @param errorMessage Optional output for write failures.
 * @return `true` on success.
 */
bool saveConfig(const QString &path, const AppConfig &config, QString *errorMessage = nullptr);

/**
 * @brief Returns the supported dotted keys for config mutation.
 * @return Supported config keys in user-facing display order.
 */
QStringList supportedConfigKeys();

/**
 * @brief Applies a single dotted-key config value with validation.
 * @param config Config snapshot to mutate.
 * @param key Canonical dotted config key.
 * @param value Text value to parse and validate.
 * @param errorMessage Optional output for validation failures.
 * @return `true` when the key is supported and the value is valid.
 */
bool applyConfigValue(AppConfig *config, QStringView key, const QString &value, QString *errorMessage = nullptr);
