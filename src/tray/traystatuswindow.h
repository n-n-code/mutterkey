#pragma once

#include "control/daemoncontrolclient.h"

#include <QWidget>

class QLabel;
class QPlainTextEdit;

/**
 * @file
 * @brief Basic tray-shell status window backed by daemon control requests.
 */

/**
 * @brief Read-only status window for the initial tray-shell slice.
 */
class TrayStatusWindow final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Creates the status window and performs an initial refresh.
     * @param client Optional non-owning daemon session used for refreshes.
     * @param parent Optional parent widget.
     */
    explicit TrayStatusWindow(DaemonControlSession *client = nullptr, QWidget *parent = nullptr);
public Q_SLOTS:
    /**
     * @brief Refreshes the window from the daemon control API.
     */
    void refresh();

private:
    void setOfflineState(const QString &message);

    LocalDaemonControlSession m_ownedSession;
    DaemonControlSession *m_session = nullptr;
    QLabel *m_connectionValue = nullptr;
    QLabel *m_configPathValue = nullptr;
    QLabel *m_shortcutValue = nullptr;
    QLabel *m_modelPathValue = nullptr;
    QPlainTextEdit *m_statusJsonView = nullptr;
};
