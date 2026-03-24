#include "config.h"

#include <array>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>

extern "C" {
#include <whisper.h>
}

Q_LOGGING_CATEGORY(configLog, "mutterkey.config")

namespace {

constexpr int kDefaultAudioSampleRate = 16000;
constexpr int kDefaultAudioChannels = 1;
constexpr double kDefaultMinimumSeconds = 0.25;
constexpr int kMaxAudioChannels = 8;

QString readString(const QJsonObject &object, QStringView key, const QString &fallback)
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString() : fallback;
}

int readInt(const QJsonObject &object, QStringView key, int fallback)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toInt(fallback) : fallback;
}

double readDouble(const QJsonObject &object, QStringView key, double fallback)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toDouble(fallback) : fallback;
}

bool readBool(const QJsonObject &object, QStringView key, bool fallback)
{
    const QJsonValue value = object.value(key);
    return value.isBool() ? value.toBool(fallback) : fallback;
}

QString defaultModelDirectory()
{
    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dataRoot).filePath(QStringLiteral("models"));
}

void warnAboutInvalidValue(const QString &path,
                           QStringView field,
                           const QString &reason,
                           const QString &fallbackDescription)
{
    qCWarning(configLog).noquote()
        << QStringLiteral("Ignoring invalid config value in %1 for %2: %3. Using %4.")
               .arg(path, field, reason, fallbackDescription);
}

QString sanitizedNonEmptyString(const QString &value)
{
    QString trimmedValue = value.trimmed();
    return trimmedValue;
}

QString normalizedLanguageValue(const QString &value)
{
    return value.trimmed().toLower();
}

bool resolveWhisperLanguage(const QString &value, QString *resolvedLanguage)
{
    const QString normalizedValue = normalizedLanguageValue(value);
    if (normalizedValue.isEmpty()) {
        return false;
    }

    if (normalizedValue == QStringLiteral("auto")) {
        if (resolvedLanguage != nullptr) {
            *resolvedLanguage = normalizedValue;
        }
        return true;
    }

    for (int languageId = 0; languageId <= whisper_lang_max_id(); ++languageId) {
        const char *languageCode = whisper_lang_str(languageId);
        const char *languageName = whisper_lang_str_full(languageId);
        if (languageCode == nullptr) {
            continue;
        }

        if (normalizedValue == QString::fromUtf8(languageCode)
            || (languageName != nullptr && normalizedValue == QString::fromUtf8(languageName))) {
            if (resolvedLanguage != nullptr) {
                *resolvedLanguage = QString::fromUtf8(languageCode);
            }
            return true;
        }
    }

    return false;
}

bool parseBoolValue(const QString &value, bool *parsedValue)
{
    if (parsedValue == nullptr) {
        return false;
    }

    const QString normalizedValue = value.trimmed().toLower();
    if (normalizedValue == QStringLiteral("true") || normalizedValue == QStringLiteral("1")
        || normalizedValue == QStringLiteral("yes") || normalizedValue == QStringLiteral("on")) {
        *parsedValue = true;
        return true;
    }

    if (normalizedValue == QStringLiteral("false") || normalizedValue == QStringLiteral("0")
        || normalizedValue == QStringLiteral("no") || normalizedValue == QStringLiteral("off")) {
        *parsedValue = false;
        return true;
    }

    return false;
}

int validatedAudioSampleRate(const QString &path, int sampleRate)
{
    if (sampleRate > 0) {
        return sampleRate;
    }

    warnAboutInvalidValue(path,
                          QStringLiteral("audio.sample_rate"),
                          QStringLiteral("value must be greater than 0"),
                          QString::number(kDefaultAudioSampleRate));
    return kDefaultAudioSampleRate;
}

int validatedAudioChannels(const QString &path, int channels)
{
    if (channels > 0 && channels <= kMaxAudioChannels) {
        return channels;
    }

    warnAboutInvalidValue(path,
                          QStringLiteral("audio.channels"),
                          QStringLiteral("value must be between 1 and 8"),
                          QString::number(kDefaultAudioChannels));
    return kDefaultAudioChannels;
}

double validatedMinimumSeconds(const QString &path, double minimumSeconds)
{
    if (minimumSeconds >= 0.0) {
        return minimumSeconds;
    }

    warnAboutInvalidValue(path,
                          QStringLiteral("audio.minimum_seconds"),
                          QStringLiteral("value must be 0 or greater"),
                          QString::number(kDefaultMinimumSeconds));
    return kDefaultMinimumSeconds;
}

int validatedThreads(const QString &path, int threads)
{
    if (threads >= 0) {
        return threads;
    }

    warnAboutInvalidValue(path,
                          QStringLiteral("transcriber.threads"),
                          QStringLiteral("value must be 0 or greater"),
                          QStringLiteral("0"));
    return 0;
}

QString normalizedLogLevel(const QString &logLevel)
{
    QString normalizedLevel = logLevel.trimmed().toUpper();
    return normalizedLevel;
}

bool isSupportedLogLevel(const QString &logLevel)
{
    static const QStringList kAllowedLevels{
        QStringLiteral("DEBUG"),
        QStringLiteral("INFO"),
        QStringLiteral("WARNING"),
        QStringLiteral("ERROR"),
    };

    return kAllowedLevels.contains(logLevel);
}

bool setShortcutSequence(AppConfig *config, const QString &value, QString *errorMessage)
{
    const QString trimmedValue = sanitizedNonEmptyString(value);
    if (trimmedValue.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("shortcut.sequence may not be empty");
        }
        return false;
    }

    config->shortcut.sequence = trimmedValue;
    return true;
}

bool setAudioSampleRate(AppConfig *config, const QString &value, QString *errorMessage)
{
    bool ok = false;
    const int sampleRate = value.trimmed().toInt(&ok);
    if (!ok || sampleRate <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("audio.sample_rate must be greater than 0");
        }
        return false;
    }

    config->audio.sampleRate = sampleRate;
    return true;
}

bool setAudioChannels(AppConfig *config, const QString &value, QString *errorMessage)
{
    bool ok = false;
    const int channels = value.trimmed().toInt(&ok);
    if (!ok || channels <= 0 || channels > kMaxAudioChannels) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("audio.channels must be between 1 and 8");
        }
        return false;
    }

    config->audio.channels = channels;
    return true;
}

bool setAudioMinimumSeconds(AppConfig *config, const QString &value, QString *errorMessage)
{
    bool ok = false;
    const double minimumSeconds = value.trimmed().toDouble(&ok);
    if (!ok || minimumSeconds < 0.0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("audio.minimum_seconds must be 0 or greater");
        }
        return false;
    }

    config->audio.minimumSeconds = minimumSeconds;
    return true;
}

bool setAudioDeviceId(AppConfig *config, const QString &value, QString *)
{
    config->audio.deviceId = value.trimmed();
    return true;
}

bool setModelPath(AppConfig *config, const QString &value, QString *errorMessage)
{
    const QString trimmedValue = sanitizedNonEmptyString(value);
    if (trimmedValue.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("transcriber.model_path may not be empty");
        }
        return false;
    }

    config->transcriber.modelPath = trimmedValue;
    return true;
}

bool setLanguage(AppConfig *config, const QString &value, QString *errorMessage)
{
    QString resolvedLanguage;
    if (!resolveWhisperLanguage(value, &resolvedLanguage)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("transcriber.language must be \"auto\" or a supported Whisper language code");
        }
        return false;
    }

    config->transcriber.language = resolvedLanguage;
    return true;
}

bool setTranslate(AppConfig *config, const QString &value, QString *errorMessage)
{
    bool translate = false;
    if (!parseBoolValue(value, &translate)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("transcriber.translate must be a boolean value");
        }
        return false;
    }

    config->transcriber.translate = translate;
    return true;
}

bool setThreads(AppConfig *config, const QString &value, QString *errorMessage)
{
    bool ok = false;
    const int threads = value.trimmed().toInt(&ok);
    if (!ok || threads < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("transcriber.threads must be 0 or greater");
        }
        return false;
    }

    config->transcriber.threads = threads;
    return true;
}

bool setWarmupOnStart(AppConfig *config, const QString &value, QString *errorMessage)
{
    bool warmupOnStart = false;
    if (!parseBoolValue(value, &warmupOnStart)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("transcriber.warmup_on_start must be a boolean value");
        }
        return false;
    }

    config->transcriber.warmupOnStart = warmupOnStart;
    return true;
}

bool setLogLevel(AppConfig *config, const QString &value, QString *errorMessage)
{
    const QString logLevel = normalizedLogLevel(value);
    if (!isSupportedLogLevel(logLevel)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("log_level must be one of DEBUG, INFO, WARNING, ERROR");
        }
        return false;
    }

    config->logLevel = logLevel;
    return true;
}

using ConfigSetter = bool (*)(AppConfig *config, const QString &value, QString *errorMessage);

struct ConfigKeyHandler {
    QLatin1StringView key;
    ConfigSetter setter;
};

constexpr std::array kConfigKeyHandlers{
    ConfigKeyHandler{.key = QLatin1StringView("shortcut.sequence"), .setter = &setShortcutSequence},
    ConfigKeyHandler{.key = QLatin1StringView("audio.sample_rate"), .setter = &setAudioSampleRate},
    ConfigKeyHandler{.key = QLatin1StringView("audio.channels"), .setter = &setAudioChannels},
    ConfigKeyHandler{.key = QLatin1StringView("audio.minimum_seconds"), .setter = &setAudioMinimumSeconds},
    ConfigKeyHandler{.key = QLatin1StringView("audio.device_id"), .setter = &setAudioDeviceId},
    ConfigKeyHandler{.key = QLatin1StringView("transcriber.model_path"), .setter = &setModelPath},
    ConfigKeyHandler{.key = QLatin1StringView("transcriber.language"), .setter = &setLanguage},
    ConfigKeyHandler{.key = QLatin1StringView("transcriber.translate"), .setter = &setTranslate},
    ConfigKeyHandler{.key = QLatin1StringView("transcriber.threads"), .setter = &setThreads},
    ConfigKeyHandler{.key = QLatin1StringView("transcriber.warmup_on_start"), .setter = &setWarmupOnStart},
    ConfigKeyHandler{.key = QLatin1StringView("log_level"), .setter = &setLogLevel},
};

} // namespace

QString defaultConfigPath()
{
    const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return QDir(configRoot).filePath(QStringLiteral("mutterkey/config.json"));
}

QString defaultModelPath()
{
    return QDir(defaultModelDirectory()).filePath(QStringLiteral("ggml-base.en.bin"));
}

AppConfig defaultAppConfig()
{
    AppConfig config;
    config.transcriber.modelPath = defaultModelPath();
    return config;
}

AppConfig loadConfig(const QString &path, QString *errorMessage)
{
    AppConfig config = defaultAppConfig();
    QFile file(path);
    if (!file.exists()) {
        return config;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not open config file: %1").arg(path);
        }
        return config;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid JSON config %1: %2").arg(path, parseError.errorString());
        }
        return config;
    }

    const QJsonObject root = document.object();
    const QJsonObject shortcut = root.value(QStringLiteral("shortcut")).toObject();
    config.shortcut.componentUnique = readString(shortcut, QStringLiteral("component_unique"), config.shortcut.componentUnique);
    config.shortcut.componentFriendly = readString(shortcut, QStringLiteral("component_friendly"), config.shortcut.componentFriendly);
    config.shortcut.actionUnique = readString(shortcut, QStringLiteral("action_unique"), config.shortcut.actionUnique);
    config.shortcut.actionFriendly = readString(shortcut, QStringLiteral("action_friendly"), config.shortcut.actionFriendly);
    const QString shortcutSequence = sanitizedNonEmptyString(readString(shortcut, QStringLiteral("sequence"), config.shortcut.sequence));
    if (shortcutSequence.isEmpty()) {
        warnAboutInvalidValue(path,
                              QStringLiteral("shortcut.sequence"),
                              QStringLiteral("value is empty"),
                              config.shortcut.sequence);
    } else {
        config.shortcut.sequence = shortcutSequence;
    }

    const QJsonObject audio = root.value(QStringLiteral("audio")).toObject();
    config.audio.sampleRate = validatedAudioSampleRate(path, readInt(audio, QStringLiteral("sample_rate"), config.audio.sampleRate));
    config.audio.channels = validatedAudioChannels(path, readInt(audio, QStringLiteral("channels"), config.audio.channels));
    config.audio.minimumSeconds =
        validatedMinimumSeconds(path, readDouble(audio, QStringLiteral("minimum_seconds"), config.audio.minimumSeconds));
    config.audio.deviceId = readString(audio, QStringLiteral("device_id"), config.audio.deviceId);

    const QJsonObject transcriber = root.value(QStringLiteral("transcriber")).toObject();
    const QString modelPath = sanitizedNonEmptyString(readString(transcriber, QStringLiteral("model_path"), defaultModelPath()));
    if (modelPath.isEmpty()) {
        warnAboutInvalidValue(path,
                              QStringLiteral("transcriber.model_path"),
                              QStringLiteral("value is empty"),
                              defaultModelPath());
    } else {
        config.transcriber.modelPath = modelPath;
    }
    config.transcriber.language = readString(transcriber, QStringLiteral("language"), config.transcriber.language);
    QString resolvedLanguage;
    if (resolveWhisperLanguage(config.transcriber.language, &resolvedLanguage)) {
        config.transcriber.language = resolvedLanguage;
    } else {
        warnAboutInvalidValue(path,
                              QStringLiteral("transcriber.language"),
                              QStringLiteral("unsupported Whisper language"),
                              defaultAppConfig().transcriber.language);
        config.transcriber.language = defaultAppConfig().transcriber.language;
    }
    config.transcriber.translate = readBool(transcriber, QStringLiteral("translate"), config.transcriber.translate);
    config.transcriber.threads = validatedThreads(path, readInt(transcriber, QStringLiteral("threads"), config.transcriber.threads));
    config.transcriber.warmupOnStart =
        readBool(transcriber, QStringLiteral("warmup_on_start"), config.transcriber.warmupOnStart);

    const QString logLevel = normalizedLogLevel(readString(root, QStringLiteral("log_level"), config.logLevel));
    if (isSupportedLogLevel(logLevel)) {
        config.logLevel = logLevel;
    } else {
        warnAboutInvalidValue(path,
                              QStringLiteral("log_level"),
                              QStringLiteral("expected one of DEBUG, INFO, WARNING, ERROR"),
                              config.logLevel);
    }

    qCInfo(configLog) << "Loaded config from" << path;
    return config;
}

QByteArray serializeConfig(const AppConfig &config)
{
    QJsonObject shortcut;
    shortcut.insert(QStringLiteral("component_unique"), config.shortcut.componentUnique);
    shortcut.insert(QStringLiteral("component_friendly"), config.shortcut.componentFriendly);
    shortcut.insert(QStringLiteral("action_unique"), config.shortcut.actionUnique);
    shortcut.insert(QStringLiteral("action_friendly"), config.shortcut.actionFriendly);
    shortcut.insert(QStringLiteral("sequence"), config.shortcut.sequence);

    QJsonObject audio;
    audio.insert(QStringLiteral("sample_rate"), config.audio.sampleRate);
    audio.insert(QStringLiteral("channels"), config.audio.channels);
    audio.insert(QStringLiteral("minimum_seconds"), config.audio.minimumSeconds);
    audio.insert(QStringLiteral("device_id"), config.audio.deviceId);

    QJsonObject transcriber;
    transcriber.insert(QStringLiteral("model_path"), config.transcriber.modelPath);
    transcriber.insert(QStringLiteral("language"), config.transcriber.language);
    transcriber.insert(QStringLiteral("translate"), config.transcriber.translate);
    transcriber.insert(QStringLiteral("threads"), config.transcriber.threads);
    transcriber.insert(QStringLiteral("warmup_on_start"), config.transcriber.warmupOnStart);

    QJsonObject root;
    root.insert(QStringLiteral("shortcut"), shortcut);
    root.insert(QStringLiteral("audio"), audio);
    root.insert(QStringLiteral("transcriber"), transcriber);
    root.insert(QStringLiteral("log_level"), config.logLevel);

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool saveConfig(const QString &path, const AppConfig &config, QString *errorMessage)
{
    const QFileInfo configFileInfo(path);
    QDir directory;
    if (!directory.mkpath(configFileInfo.absolutePath())) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not create config directory: %1").arg(configFileInfo.absolutePath());
        }
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not open config file for writing: %1").arg(path);
        }
        return false;
    }

    if (file.write(serializeConfig(config)) < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not write config file: %1").arg(path);
        }
        return false;
    }

    if (!file.commit()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not commit config file: %1").arg(path);
        }
        return false;
    }

    return true;
}

QStringList supportedConfigKeys()
{
    QStringList keys;
    keys.reserve(std::size(kConfigKeyHandlers));
    for (const ConfigKeyHandler &handler : kConfigKeyHandlers) {
        keys.append(handler.key.toString());
    }
    return keys;
}

bool applyConfigValue(AppConfig *config, QStringView key, const QString &value, QString *errorMessage)
{
    if (config == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Internal error: missing config target");
        }
        return false;
    }

    for (const ConfigKeyHandler &handler : kConfigKeyHandlers) {
        if (handler.key == key) {
            return handler.setter(config, value, errorMessage);
        }
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Unsupported config key: %1").arg(key);
    }
    return false;
}
