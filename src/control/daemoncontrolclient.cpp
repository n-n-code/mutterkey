#include "control/daemoncontrolclient.h"

#include <QLocalSocket>
#include <QUuid>

LocalDaemonControlSession::LocalDaemonControlSession(QString socketName)
    : m_socketName(std::move(socketName))
{
}

DaemonControlResponse LocalDaemonControlSession::requestRaw(DaemonControlMethod method,
                                                            int timeoutMs,
                                                            QString *transportError) const
{
    if (transportError != nullptr) {
        transportError->clear();
    }

    QLocalSocket socket;
    socket.connectToServer(m_socketName);
    if (!socket.waitForConnected(timeoutMs)) {
        if (transportError != nullptr) {
            *transportError = socket.errorString();
        }
        return {};
    }

    DaemonControlRequest request;
    request.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request.method = method;

    const QByteArray payload = serializeDaemonControlRequest(request);
    if (socket.write(payload) != payload.size() || !socket.waitForBytesWritten(timeoutMs)) {
        if (transportError != nullptr) {
            *transportError = socket.errorString();
        }
        return {};
    }

    while (!socket.canReadLine()) {
        if (!socket.waitForReadyRead(timeoutMs)) {
            if (transportError != nullptr) {
                *transportError = socket.errorString();
            }
            return {};
        }
    }

    const QByteArray responsePayload = socket.readLine();
    DaemonControlResponse response;
    QString parseError;
    if (!parseDaemonControlResponse(responsePayload, &response, &parseError)) {
        if (transportError != nullptr) {
            *transportError = parseError;
        }
        return {};
    }

    if (response.requestId != request.requestId) {
        if (transportError != nullptr) {
            *transportError = QStringLiteral("Mismatched daemon response id");
        }
        return {};
    }

    return response;
}

DaemonStatusResult LocalDaemonControlSession::fetchStatus(int timeoutMs) const
{
    QString transportError;
    const DaemonControlResponse response = requestRaw(DaemonControlMethod::GetStatus, timeoutMs, &transportError);

    DaemonStatusResult result;
    if (!transportError.isEmpty()) {
        result.errorMessage = transportError;
        return result;
    }
    if (!response.success) {
        result.errorMessage = response.errorMessage;
        return result;
    }

    QString parseError;
    if (!parseDaemonStatusSnapshot(response.result, &result.snapshot, &parseError)) {
        result.errorMessage = parseError;
        return result;
    }

    result.success = true;
    return result;
}

DaemonConfigResult LocalDaemonControlSession::fetchConfig(int timeoutMs) const
{
    QString transportError;
    const DaemonControlResponse response = requestRaw(DaemonControlMethod::GetConfig, timeoutMs, &transportError);

    DaemonConfigResult result;
    if (!transportError.isEmpty()) {
        result.errorMessage = transportError;
        return result;
    }
    if (!response.success) {
        result.errorMessage = response.errorMessage;
        return result;
    }

    QString parseError;
    if (!parseDaemonConfigSnapshot(response.result, &result.snapshot, &parseError)) {
        result.errorMessage = parseError;
        return result;
    }

    result.success = true;
    return result;
}
