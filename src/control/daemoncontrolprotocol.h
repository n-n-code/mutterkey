#pragma once

#include <cstdint>

#include <QByteArray>
#include <QJsonObject>
#include <QString>

/**
 * @file
 * @brief Local daemon control protocol types and JSON serialization helpers.
 */

/**
 * @brief Supported operations on the local daemon control socket.
 */
enum class DaemonControlMethod : std::uint8_t {
    /// Lightweight health check that returns daemon identity information.
    Ping,
    /// Returns daemon-authoritative runtime status and service diagnostics.
    GetStatus,
    /// Returns the daemon's current config snapshot and config-path metadata.
    GetConfig,
};

/**
 * @brief One line-delimited local control request.
 */
struct DaemonControlRequest {
    /// Protocol version expected by the daemon and tray client.
    int version = 1;
    /// Caller-generated correlation id echoed by the daemon response.
    QString requestId;
    /// Requested daemon operation.
    DaemonControlMethod method = DaemonControlMethod::Ping;
};

/**
 * @brief One line-delimited local control response.
 */
struct DaemonControlResponse {
    /// Protocol version returned by the daemon.
    int version = 1;
    /// Correlation id copied from the triggering request.
    QString requestId;
    /// Indicates whether the request completed successfully.
    bool success = false;
    /// Successful result object returned by the daemon.
    QJsonObject result;
    /// Human-readable error for failed requests.
    QString errorMessage;
};

/**
 * @brief Returns the local server name used by the daemon control socket.
 * @return Transport-specific socket/server name.
 */
QString daemonControlSocketName();

/**
 * @brief Converts a control method enum to its wire-format string name.
 * @param method Method value to convert.
 * @return Canonical protocol string for the method.
 */
QString daemonControlMethodToString(DaemonControlMethod method);

/**
 * @brief Parses a wire-format control method name.
 * @param value Method string from a protocol payload.
 * @param methodOut Output target for the parsed method.
 * @return `true` when the method is recognized.
 */
bool parseDaemonControlMethod(QStringView value, DaemonControlMethod *methodOut);

/**
 * @brief Serializes a request to compact line-delimited JSON.
 * @param request Request value to serialize.
 * @return UTF-8 payload terminated with `\n`.
 */
QByteArray serializeDaemonControlRequest(const DaemonControlRequest &request);

/**
 * @brief Serializes a response to compact line-delimited JSON.
 * @param response Response value to serialize.
 * @return UTF-8 payload terminated with `\n`.
 */
QByteArray serializeDaemonControlResponse(const DaemonControlResponse &response);

/**
 * @brief Parses a line-delimited JSON request payload.
 * @param payload UTF-8 request payload.
 * @param requestOut Output target for the parsed request.
 * @param errorMessage Optional output for parse failures.
 * @return `true` when the payload is valid.
 */
bool parseDaemonControlRequest(const QByteArray &payload, DaemonControlRequest *requestOut, QString *errorMessage = nullptr);

/**
 * @brief Parses a line-delimited JSON response payload.
 * @param payload UTF-8 response payload.
 * @param responseOut Output target for the parsed response.
 * @param errorMessage Optional output for parse failures.
 * @return `true` when the payload is valid.
 */
bool parseDaemonControlResponse(const QByteArray &payload,
                                DaemonControlResponse *responseOut,
                                QString *errorMessage = nullptr);
