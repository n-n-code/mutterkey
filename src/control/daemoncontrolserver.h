#pragma once

#include "control/daemoncontrolprotocol.h"
#include "control/daemoncontroltypes.h"
#include "config.h"
#include "service.h"

#include <QObject>

class QLocalServer;

/**
 * @file
 * @brief Local socket server that exposes a narrow daemon control API.
 */

/**
 * @brief Serves daemon status and config requests over a local socket.
 */
class DaemonControlServer final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates the local control server for a running daemon.
     * @param configPath Resolved daemon config path.
     * @param config Startup config snapshot exposed through the control API.
     * @param service Non-owning pointer to the running daemon service.
     * @param parent Optional QObject parent.
     */
    explicit DaemonControlServer(QString configPath,
                                 AppConfig config,
                                 const MutterkeyService *service,
                                 QObject *parent = nullptr);
    /**
     * @brief Creates the local control server for a running daemon on a custom socket name.
     * @param configPath Resolved daemon config path.
     * @param config Startup config snapshot exposed through the control API.
     * @param service Non-owning pointer to the running daemon service.
     * @param socketName Injected local socket name for deterministic tests or alternate local control endpoints.
     * @param parent Optional QObject parent.
     */
    explicit DaemonControlServer(QString configPath,
                                 AppConfig config,
                                 const MutterkeyService *service,
                                 QString socketName,
                                 QObject *parent);
    /**
     * @brief Stops the local server and releases the socket name.
     */
    ~DaemonControlServer() override;

    Q_DISABLE_COPY_MOVE(DaemonControlServer)

    /**
     * @brief Starts listening on the local daemon control socket.
     * @param errorMessage Optional output for startup failures.
     * @return `true` when the server is listening.
     */
    bool start(QString *errorMessage = nullptr);

    /**
     * @brief Stops listening and removes the local socket endpoint.
     */
    void stop();

private Q_SLOTS:
    void onNewConnection();
    void onSocketReadyRead();

private:
    [[nodiscard]] QByteArray handleRequest(const QByteArray &payload) const;
    [[nodiscard]] DaemonStatusSnapshot buildStatusSnapshot() const;
    [[nodiscard]] DaemonConfigSnapshot buildConfigSnapshot() const;

    QString m_configPath;
    AppConfig m_config;
    const MutterkeyService *m_service = nullptr;
    /// Socket name used by both the listener and ping diagnostics payloads.
    QString m_socketName;
    QLocalServer *m_server = nullptr;
};
