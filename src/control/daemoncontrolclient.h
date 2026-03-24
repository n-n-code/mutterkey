#pragma once

#include "control/daemoncontrolprotocol.h"
#include "control/daemoncontroltypes.h"

/**
 * @file
 * @brief Synchronous client for the local Mutterkey daemon control socket.
 */

/**
 * @brief Result of a daemon status query.
 */
struct DaemonStatusResult {
    /// Indicates whether the daemon query completed successfully.
    bool success = false;
    /// Parsed typed snapshot when `success` is `true`.
    DaemonStatusSnapshot snapshot;
    /// Human-readable failure reason when `success` is `false`.
    QString errorMessage;
};

/**
 * @brief Result of a daemon config query.
 */
struct DaemonConfigResult {
    /// Indicates whether the daemon query completed successfully.
    bool success = false;
    /// Parsed typed snapshot when `success` is `true`.
    DaemonConfigSnapshot snapshot;
    /// Human-readable failure reason when `success` is `false`.
    QString errorMessage;
};

/**
 * @brief Interface used by tray-facing code to query daemon state.
 */
class DaemonControlSession
{
public:
    DaemonControlSession() = default;
    virtual ~DaemonControlSession() = default;
    DaemonControlSession(const DaemonControlSession &) = delete;
    DaemonControlSession &operator=(const DaemonControlSession &) = delete;
    DaemonControlSession(DaemonControlSession &&) = delete;
    DaemonControlSession &operator=(DaemonControlSession &&) = delete;

    /**
     * @brief Requests a typed daemon status snapshot.
     * @param timeoutMs Connect, write, and read timeout in milliseconds.
     * @return Typed status result.
     */
    [[nodiscard]] virtual DaemonStatusResult fetchStatus(int timeoutMs) const = 0;

    /**
     * @brief Requests a typed daemon config snapshot.
     * @param timeoutMs Connect, write, and read timeout in milliseconds.
     * @return Typed config result.
     */
    [[nodiscard]] virtual DaemonConfigResult fetchConfig(int timeoutMs) const = 0;
};

/**
 * @brief Concrete local-socket implementation of the daemon control session API.
 */
class LocalDaemonControlSession final : public DaemonControlSession
{
public:
    /**
     * @brief Creates a session bound to a named local socket endpoint.
     * @param socketName Local socket name to connect to.
     */
    explicit LocalDaemonControlSession(QString socketName = daemonControlSocketName());

    /**
     * @brief Requests a typed daemon status snapshot.
     * @param timeoutMs Connect, write, and read timeout in milliseconds.
     * @return Typed status result.
     */
    [[nodiscard]] DaemonStatusResult fetchStatus(int timeoutMs) const override;

    /**
     * @brief Requests a typed daemon config snapshot.
     * @param timeoutMs Connect, write, and read timeout in milliseconds.
     * @return Typed config result.
     */
    [[nodiscard]] DaemonConfigResult fetchConfig(int timeoutMs) const override;

private:
    [[nodiscard]] DaemonControlResponse requestRaw(DaemonControlMethod method,
                                                   int timeoutMs,
                                                   QString *transportError = nullptr) const;

    /// Local socket endpoint name used for daemon requests.
    QString m_socketName;
};
