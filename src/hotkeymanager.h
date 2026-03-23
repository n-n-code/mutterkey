#pragma once

#include "config.h"

#include <QJsonObject>
#include <QKeySequence>
#include <QObject>

class QAction;

/**
 * @file
 * @brief KDE global shortcut registration and activation tracking.
 */

/**
 * @brief Wraps KGlobalAccel registration for Mutterkey's push-to-talk shortcut.
 *
 * The manager owns the QAction used for registration and translates KDE active
 * state changes into press and release signals for the service layer.
 */
class HotkeyManager final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates a hotkey manager with a fixed shortcut configuration.
     * @param config Shortcut registration settings copied into the manager.
     * @param parent Optional QObject parent.
     */
    explicit HotkeyManager(ShortcutConfig config, QObject *parent = nullptr);

    /**
     * @brief Unregisters the shortcut and releases owned KDE objects.
     */
    ~HotkeyManager() override;

    Q_DISABLE_COPY_MOVE(HotkeyManager)

    /**
     * @brief Registers the configured shortcut with KDE global accel.
     * @param errorMessage Optional output for registration failures.
     * @return `true` when registration succeeded.
     */
    bool registerShortcut(QString *errorMessage = nullptr);

    /**
     * @brief Unregisters the shortcut if it is currently active.
     */
    void unregisterShortcut();

    /**
     * @brief Simulates shortcut invocation through the KDE action object.
     * @param errorMessage Optional output for invocation failures.
     * @return `true` when the action could be invoked.
     */
    bool invokeShortcut(QString *errorMessage = nullptr);

    /**
     * @brief Returns diagnostic state about registration and event counts.
     * @return JSON object suitable for `diagnose` mode output.
     */
    [[nodiscard]] QJsonObject diagnostics() const;

signals:
    /**
     * @brief Emitted when the registered shortcut becomes active.
     */
    void shortcutPressed();

    /**
     * @brief Emitted when the registered shortcut is released.
     */
    void shortcutReleased();

private slots:
    /**
     * @brief Handles KDE global shortcut active-state changes.
     * @param action Action whose state changed.
     * @param active New active state reported by KDE.
     */
    void onGlobalShortcutActiveChanged(QAction *action, bool active);

private:
    /**
     * @brief Parses the configured key sequence into a Qt key sequence object.
     * @param sequence Output location for the parsed sequence.
     * @param errorMessage Optional output for parse failures.
     * @return `true` when the configured sequence is valid.
     */
    bool parseSequence(QKeySequence *sequence, QString *errorMessage = nullptr) const;

    /// Immutable shortcut registration settings.
    ShortcutConfig m_config;
    /// Owned QAction registered with KGlobalAccel.
    QAction *m_action = nullptr;
    /// Parsed sequence assigned during successful registration.
    QKeySequence m_assignedSequence;
    /// Number of observed press transitions for diagnostics.
    int m_pressedEvents = 0;
    /// Number of observed release transitions for diagnostics.
    int m_releasedEvents = 0;
    /// Tracks whether registration is currently active.
    bool m_registered = false;
    /// Last known active state to suppress duplicate transitions.
    bool m_shortcutActive = false;
};
