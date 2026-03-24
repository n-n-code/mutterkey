#pragma once

#include "config.h"

#include <QJsonObject>
#include <QString>

/**
 * @file
 * @brief Typed daemon-control payloads used above the transport layer.
 */

/**
 * @brief Daemon-owned runtime status snapshot for tray and CLI clients.
 */
struct DaemonStatusSnapshot {
    /// Indicates whether the daemon reported itself as running.
    bool daemonRunning = false;
    /// Resolved config path used by the daemon process.
    QString configPath;
    /// Indicates whether the config path currently exists on disk.
    bool configExists = false;
    /// Daemon-owned service diagnostics payload for troubleshooting and status UI.
    QJsonObject serviceDiagnostics;
};

/**
 * @brief Daemon-owned config snapshot for tray and CLI clients.
 */
struct DaemonConfigSnapshot {
    /// Resolved config path used by the daemon process.
    QString configPath;
    /// Indicates whether the config path currently exists on disk.
    bool configExists = false;
    /// Resolved config snapshot exposed to control-plane clients.
    AppConfig config;
};

/**
 * @brief Converts a typed daemon status snapshot to its transport JSON shape.
 * @param snapshot Snapshot to serialize.
 * @return JSON object suitable for the daemon control protocol payload.
 */
QJsonObject daemonStatusSnapshotToJsonObject(const DaemonStatusSnapshot &snapshot);

/**
 * @brief Converts a typed daemon config snapshot to its transport JSON shape.
 * @param snapshot Snapshot to serialize.
 * @return JSON object suitable for the daemon control protocol payload.
 */
QJsonObject daemonConfigSnapshotToJsonObject(const DaemonConfigSnapshot &snapshot);

/**
 * @brief Parses a typed daemon status snapshot from a protocol payload object.
 * @param object JSON object returned by the daemon control protocol.
 * @param snapshotOut Output target for the parsed snapshot.
 * @param errorMessage Optional output for parse failures.
 * @return `true` when the payload is valid.
 */
bool parseDaemonStatusSnapshot(const QJsonObject &object, DaemonStatusSnapshot *snapshotOut, QString *errorMessage = nullptr);

/**
 * @brief Parses a typed daemon config snapshot from a protocol payload object.
 * @param object JSON object returned by the daemon control protocol.
 * @param snapshotOut Output target for the parsed snapshot.
 * @param errorMessage Optional output for parse failures.
 * @return `true` when the payload is valid.
 */
bool parseDaemonConfigSnapshot(const QJsonObject &object, DaemonConfigSnapshot *snapshotOut, QString *errorMessage = nullptr);
