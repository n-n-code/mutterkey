#include "control/daemoncontroltypes.h"

#include <QJsonValue>

QJsonObject daemonStatusSnapshotToJsonObject(const DaemonStatusSnapshot &snapshot)
{
    QJsonObject object;
    object.insert(QStringLiteral("daemon_running"), snapshot.daemonRunning);
    object.insert(QStringLiteral("config_path"), snapshot.configPath);
    object.insert(QStringLiteral("config_exists"), snapshot.configExists);
    object.insert(QStringLiteral("service"), snapshot.serviceDiagnostics);
    return object;
}

QJsonObject daemonConfigSnapshotToJsonObject(const DaemonConfigSnapshot &snapshot)
{
    QJsonObject object;
    object.insert(QStringLiteral("config_path"), snapshot.configPath);
    object.insert(QStringLiteral("config_exists"), snapshot.configExists);
    object.insert(QStringLiteral("config"), configToJsonObject(snapshot.config));
    return object;
}

bool parseDaemonStatusSnapshot(const QJsonObject &object, DaemonStatusSnapshot *snapshotOut, QString *errorMessage)
{
    if (snapshotOut == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Internal error: missing status snapshot output target");
        }
        return false;
    }

    const QJsonValue daemonRunningValue = object.value(QStringLiteral("daemon_running"));
    const QJsonValue configPathValue = object.value(QStringLiteral("config_path"));
    const QJsonValue configExistsValue = object.value(QStringLiteral("config_exists"));
    const QJsonValue serviceValue = object.value(QStringLiteral("service"));
    if (!daemonRunningValue.isBool() || !configPathValue.isString() || !configExistsValue.isBool() || !serviceValue.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Malformed daemon status payload");
        }
        return false;
    }

    snapshotOut->daemonRunning = daemonRunningValue.toBool();
    snapshotOut->configPath = configPathValue.toString();
    snapshotOut->configExists = configExistsValue.toBool();
    snapshotOut->serviceDiagnostics = serviceValue.toObject();
    return true;
}

bool parseDaemonConfigSnapshot(const QJsonObject &object, DaemonConfigSnapshot *snapshotOut, QString *errorMessage)
{
    if (snapshotOut == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Internal error: missing config snapshot output target");
        }
        return false;
    }

    const QJsonValue configPathValue = object.value(QStringLiteral("config_path"));
    const QJsonValue configExistsValue = object.value(QStringLiteral("config_exists"));
    const QJsonValue configValue = object.value(QStringLiteral("config"));
    if (!configPathValue.isString() || !configExistsValue.isBool() || !configValue.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Malformed daemon config payload");
        }
        return false;
    }

    snapshotOut->configPath = configPathValue.toString();
    snapshotOut->configExists = configExistsValue.toBool();
    snapshotOut->config = loadConfigObject(configValue.toObject(), QStringLiteral("daemon control config payload"));
    return true;
}
