#include "control/daemoncontrolserver.h"

#include "control/daemoncontrolprotocol.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>

DaemonControlServer::DaemonControlServer(QString configPath,
                                         AppConfig config,
                                         const MutterkeyService *service,
                                         QObject *parent)
    : QObject(parent)
    , m_configPath(std::move(configPath))
    , m_config(std::move(config))
    , m_service(service)
    , m_server(new QLocalServer(this))
{
    connect(m_server, &QLocalServer::newConnection, this, &DaemonControlServer::onNewConnection);
}

DaemonControlServer::~DaemonControlServer()
{
    stop();
}

bool DaemonControlServer::start(QString *errorMessage)
{
    QLocalServer::removeServer(daemonControlSocketName());
    if (m_server->listen(daemonControlSocketName())) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = m_server->errorString();
    }
    return false;
}

void DaemonControlServer::stop()
{
    if (m_server->isListening()) {
        m_server->close();
        QLocalServer::removeServer(daemonControlSocketName());
    }
}

void DaemonControlServer::onNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        socket->setParent(this);
        connect(socket, &QLocalSocket::readyRead, this, &DaemonControlServer::onSocketReadyRead);
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void DaemonControlServer::onSocketReadyRead()
{
    auto *socket = qobject_cast<QLocalSocket *>(sender());
    if (socket == nullptr) {
        return;
    }

    while (socket->canReadLine()) {
        const QByteArray response = handleRequest(socket->readLine());
        socket->write(response);
        socket->flush();
    }
}

QByteArray DaemonControlServer::handleRequest(const QByteArray &payload) const
{
    DaemonControlRequest request;
    QString parseError;
    if (!parseDaemonControlRequest(payload, &request, &parseError)) {
        DaemonControlResponse response;
        response.requestId = QStringLiteral("invalid");
        response.errorMessage = parseError;
        return serializeDaemonControlResponse(response);
    }

    DaemonControlResponse response;
    response.requestId = request.requestId;
    response.success = true;

    switch (request.method) {
    case DaemonControlMethod::Ping:
        response.result.insert(QStringLiteral("application"), QCoreApplication::applicationName());
        response.result.insert(QStringLiteral("socket_name"), daemonControlSocketName());
        break;
    case DaemonControlMethod::GetStatus:
        response.result = daemonStatusSnapshotToJsonObject(buildStatusSnapshot());
        break;
    case DaemonControlMethod::GetConfig:
        response.result = daemonConfigSnapshotToJsonObject(buildConfigSnapshot());
        break;
    }

    return serializeDaemonControlResponse(response);
}

DaemonStatusSnapshot DaemonControlServer::buildStatusSnapshot() const
{
    DaemonStatusSnapshot snapshot;
    snapshot.daemonRunning = true;
    snapshot.configPath = m_configPath;
    snapshot.configExists = QFileInfo::exists(m_configPath);
    snapshot.serviceDiagnostics = m_service != nullptr ? m_service->diagnostics() : QJsonObject{};
    return snapshot;
}

DaemonConfigSnapshot DaemonControlServer::buildConfigSnapshot() const
{
    DaemonConfigSnapshot snapshot;
    snapshot.configPath = m_configPath;
    snapshot.configExists = QFileInfo::exists(m_configPath);
    snapshot.config = m_config;
    return snapshot;
}
