#pragma once

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

private slots:
    void onNewConnection();
    void onSocketReadyRead();

private:
    [[nodiscard]] QByteArray handleRequest(const QByteArray &payload) const;
    [[nodiscard]] DaemonStatusSnapshot buildStatusSnapshot() const;
    [[nodiscard]] DaemonConfigSnapshot buildConfigSnapshot() const;

    QString m_configPath;
    AppConfig m_config;
    const MutterkeyService *m_service = nullptr;
    QLocalServer *m_server = nullptr;
};
