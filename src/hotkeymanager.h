#pragma once

#include "config.h"

#include <QJsonObject>
#include <QKeySequence>
#include <QObject>

class QAction;

class HotkeyManager final : public QObject
{
    Q_OBJECT

public:
    explicit HotkeyManager(ShortcutConfig config, QObject *parent = nullptr);
    ~HotkeyManager() override;

    Q_DISABLE_COPY_MOVE(HotkeyManager)

    bool registerShortcut(QString *errorMessage = nullptr);
    void unregisterShortcut();
    bool invokeShortcut(QString *errorMessage = nullptr);
    [[nodiscard]] QJsonObject diagnostics() const;

signals:
    void shortcutPressed();
    void shortcutReleased();

private slots:
    void onGlobalShortcutActiveChanged(QAction *action, bool active);

private:
    bool parseSequence(QKeySequence *sequence, QString *errorMessage = nullptr) const;

    ShortcutConfig m_config;
    QAction *m_action = nullptr;
    QKeySequence m_assignedSequence;
    int m_pressedEvents = 0;
    int m_releasedEvents = 0;
    bool m_registered = false;
    bool m_shortcutActive = false;
};
