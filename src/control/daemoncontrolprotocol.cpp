#include "control/daemoncontrolprotocol.h"

#include <QJsonDocument>
#include <QJsonValue>

namespace {

constexpr int kDaemonControlProtocolVersion = 1;
constexpr auto kSocketName = "mutterkey-daemon-control-v1";

bool readVersion(const QJsonObject &object, int *versionOut, QString *errorMessage)
{
    if (versionOut == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Internal error: missing version output target");
        }
        return false;
    }

    const QJsonValue versionValue = object.value(QStringLiteral("version"));
    if (!versionValue.isDouble()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Missing numeric protocol version");
        }
        return false;
    }

    const int version = versionValue.toInt(-1);
    if (version != kDaemonControlProtocolVersion) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unsupported protocol version: %1").arg(version);
        }
        return false;
    }

    *versionOut = version;
    return true;
}

bool parseObjectDocument(const QByteArray &payload, QJsonObject *objectOut, QString *errorMessage)
{
    if (objectOut == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Internal error: missing JSON object output target");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload.trimmed(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid JSON payload: %1").arg(parseError.errorString());
        }
        return false;
    }

    *objectOut = document.object();
    return true;
}

} // namespace

QString daemonControlSocketName()
{
    return QString::fromLatin1(kSocketName);
}

QString daemonControlMethodToString(DaemonControlMethod method)
{
    switch (method) {
    case DaemonControlMethod::Ping:
        return QStringLiteral("ping");
    case DaemonControlMethod::GetStatus:
        return QStringLiteral("get_status");
    case DaemonControlMethod::GetConfig:
        return QStringLiteral("get_config");
    }

    return QStringLiteral("unknown");
}

bool parseDaemonControlMethod(QStringView value, DaemonControlMethod *methodOut)
{
    if (methodOut == nullptr) {
        return false;
    }

    if (value == QStringLiteral("ping")) {
        *methodOut = DaemonControlMethod::Ping;
        return true;
    }
    if (value == QStringLiteral("get_status")) {
        *methodOut = DaemonControlMethod::GetStatus;
        return true;
    }
    if (value == QStringLiteral("get_config")) {
        *methodOut = DaemonControlMethod::GetConfig;
        return true;
    }

    return false;
}

QByteArray serializeDaemonControlRequest(const DaemonControlRequest &request)
{
    QJsonObject object;
    object.insert(QStringLiteral("version"), request.version);
    object.insert(QStringLiteral("request_id"), request.requestId);
    object.insert(QStringLiteral("method"), daemonControlMethodToString(request.method));
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

QByteArray serializeDaemonControlResponse(const DaemonControlResponse &response)
{
    QJsonObject object;
    object.insert(QStringLiteral("version"), response.version);
    object.insert(QStringLiteral("request_id"), response.requestId);
    object.insert(QStringLiteral("ok"), response.success);
    if (response.success) {
        object.insert(QStringLiteral("result"), response.result);
    } else {
        object.insert(QStringLiteral("error"), response.errorMessage);
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

bool parseDaemonControlRequest(const QByteArray &payload, DaemonControlRequest *requestOut, QString *errorMessage)
{
    if (requestOut == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Internal error: missing request output target");
        }
        return false;
    }

    QJsonObject object;
    if (!parseObjectDocument(payload, &object, errorMessage)) {
        return false;
    }

    if (!readVersion(object, &requestOut->version, errorMessage)) {
        return false;
    }

    const QJsonValue requestIdValue = object.value(QStringLiteral("request_id"));
    if (!requestIdValue.isString() || requestIdValue.toString().trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Missing non-empty request_id");
        }
        return false;
    }
    requestOut->requestId = requestIdValue.toString().trimmed();

    const QJsonValue methodValue = object.value(QStringLiteral("method"));
    if (!methodValue.isString()
        || !parseDaemonControlMethod(methodValue.toString(), &requestOut->method)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unsupported daemon control method");
        }
        return false;
    }

    return true;
}

bool parseDaemonControlResponse(const QByteArray &payload, DaemonControlResponse *responseOut, QString *errorMessage)
{
    if (responseOut == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Internal error: missing response output target");
        }
        return false;
    }

    QJsonObject object;
    if (!parseObjectDocument(payload, &object, errorMessage)) {
        return false;
    }

    if (!readVersion(object, &responseOut->version, errorMessage)) {
        return false;
    }

    const QJsonValue requestIdValue = object.value(QStringLiteral("request_id"));
    if (!requestIdValue.isString() || requestIdValue.toString().trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Missing non-empty request_id");
        }
        return false;
    }
    responseOut->requestId = requestIdValue.toString().trimmed();

    const QJsonValue okValue = object.value(QStringLiteral("ok"));
    if (!okValue.isBool()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Missing boolean ok field");
        }
        return false;
    }

    responseOut->success = okValue.toBool();
    responseOut->result = QJsonObject{};
    responseOut->errorMessage.clear();

    if (responseOut->success) {
        const QJsonValue resultValue = object.value(QStringLiteral("result"));
        if (!resultValue.isObject()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Successful response is missing result object");
            }
            return false;
        }
        responseOut->result = resultValue.toObject();
        return true;
    }

    const QJsonValue errorValue = object.value(QStringLiteral("error"));
    if (!errorValue.isString() || errorValue.toString().trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed response is missing error text");
        }
        return false;
    }

    responseOut->errorMessage = errorValue.toString().trimmed();
    return true;
}
