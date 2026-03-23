#pragma once

#include <QString>

// Runtime defaults are intentionally centralized here so the loader, tests, and any
// future CLI/config UI work from the same baseline values.
QString defaultConfigPath();
QString defaultModelPath();

struct ShortcutConfig {
    QString componentUnique = QStringLiteral("mutterkey");
    QString componentFriendly = QStringLiteral("Mutterkey");
    QString actionUnique = QStringLiteral("push_to_talk");
    QString actionFriendly = QStringLiteral("Push to Talk");
    QString sequence = QStringLiteral("F8");
};

struct AudioConfig {
    int sampleRate = 16000;
    int channels = 1;
    double minimumSeconds = 0.25;
    QString deviceId;
};

struct TranscriberConfig {
    QString modelPath;
    QString language = QStringLiteral("en");
    bool translate = false;
    int threads = 0;
    bool warmupOnStart = false;
};

struct AppConfig {
    ShortcutConfig shortcut;
    AudioConfig audio;
    TranscriberConfig transcriber;
    QString logLevel = QStringLiteral("INFO");
};

AppConfig loadConfig(const QString &path, QString *errorMessage = nullptr);
