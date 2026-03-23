#include "config.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QStandardPaths>

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

AppConfig loadConfig(const QString &path, QString *errorMessage)
{
    AppConfig config;
    config.transcriber.modelPath = defaultModelPath();
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
