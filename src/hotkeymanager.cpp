#include "hotkeymanager.h"

#include <KGlobalAccel>

#include <QAction>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QTimer>

#include <utility>

Q_LOGGING_CATEGORY(hotkeyLog, "mutterkey.hotkey")

namespace {

QJsonArray sequenceToJson(const QKeySequence &sequence)
{
    QJsonArray array;
    for (int index = 0; index < sequence.count(); ++index) {
        array.append(sequence[index].toCombined());
    }
    return array;
}

QString sequenceToText(const QList<QKeySequence> &sequences)
{
    QStringList parts;
    parts.reserve(sequences.size());
    for (const QKeySequence &sequence : sequences) {
        if (!sequence.isEmpty()) {
            parts.append(sequence.toString(QKeySequence::PortableText));
        }
    }
    return parts.join(QStringLiteral(", "));
}

QKeySequence parseKeySequence(const QString &sequenceText)
{
    // Try the stable textual forms first so config values round-trip cleanly across
    // locales, then fall back to Qt's more permissive parser as a last resort.
    const QKeySequence portable = QKeySequence::fromString(sequenceText, QKeySequence::PortableText);
    if (!portable.isEmpty()) {
        return portable;
    }

    const QKeySequence native = QKeySequence::fromString(sequenceText, QKeySequence::NativeText);
    if (!native.isEmpty()) {
        return native;
    }

    const QString trimmedSequence = sequenceText.trimmed();
    if (trimmedSequence.size() == 1) {
        const QChar character = trimmedSequence.front();
        if (!character.isNull() && !character.isSpace()) {
            return QKeySequence(character.unicode());
        }
    }

    return QKeySequence(sequenceText);
}

QString describeConflicts(const QKeySequence &requestedSequence)
{
    const QList<KGlobalShortcutInfo> conflicts =
        KGlobalAccel::globalShortcutsByKey(requestedSequence, KGlobalAccel::Equal);
    if (conflicts.isEmpty()) {
        return {};
    }

    QStringList descriptions;
    descriptions.reserve(conflicts.size());
    for (const KGlobalShortcutInfo &conflict : conflicts) {
        descriptions.append(QStringLiteral("%1/%2")
                                .arg(conflict.componentUniqueName(), conflict.uniqueName()));
    }
    descriptions.removeDuplicates();
    return descriptions.join(QStringLiteral(", "));
}

} // namespace

HotkeyManager::HotkeyManager(ShortcutConfig config, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_action(new QAction(this))
{
    m_action->setObjectName(m_config.actionUnique);
    m_action->setText(m_config.actionFriendly);

    connect(KGlobalAccel::self(),
            &KGlobalAccel::globalShortcutActiveChanged,
            this,
            &HotkeyManager::onGlobalShortcutActiveChanged);
}

HotkeyManager::~HotkeyManager()
{
    unregisterShortcut();
}

bool HotkeyManager::registerShortcut(QString *errorMessage)
{
    if (m_registered) {
        return true;
    }

    if (m_action == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Hotkey action is not available");
        }
        return false;
    }

    QKeySequence requestedSequence;
    if (!parseSequence(&requestedSequence, errorMessage)) {
        return false;
    }

    m_action->setProperty("componentName", m_config.componentUnique);
    m_action->setProperty("componentDisplayName", m_config.componentFriendly);

    const QList<QKeySequence> shortcuts{requestedSequence};
    if (!KGlobalAccel::self()->setDefaultShortcut(m_action, shortcuts, KGlobalAccel::NoAutoloading)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not register the default KGlobalAccel shortcut");
        }
        return false;
    }

    if (!KGlobalAccel::self()->setShortcut(m_action, shortcuts, KGlobalAccel::NoAutoloading)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not register the active KGlobalAccel shortcut");
        }
        KGlobalAccel::self()->removeAllShortcuts(m_action);
        return false;
    }

    const QList<QKeySequence> assignedShortcuts = KGlobalAccel::self()->shortcut(m_action);
    m_assignedSequence = assignedShortcuts.isEmpty() ? QKeySequence() : assignedShortcuts.first();
    if (m_assignedSequence.isEmpty()) {
        KGlobalAccel::self()->removeAllShortcuts(m_action);

        if (errorMessage != nullptr) {
            const QString conflicts = describeConflicts(requestedSequence);
            if (conflicts.isEmpty()) {
                *errorMessage = QStringLiteral("KGlobalAccel did not assign an active shortcut for %1").arg(m_config.sequence);
            } else {
                *errorMessage =
                    QStringLiteral("KGlobalAccel did not assign an active shortcut for %1. Conflicts: %2")
                        .arg(m_config.sequence, conflicts);
            }
        }
        m_assignedSequence = QKeySequence();
        return false;
    }

    if (m_assignedSequence != requestedSequence) {
        qCWarning(hotkeyLog) << "Requested shortcut" << m_config.sequence << "but KDE assigned" << sequenceToText(assignedShortcuts);
    }

    m_pressedEvents = 0;
    m_releasedEvents = 0;
    m_shortcutActive = false;
    m_registered = true;

    qCInfo(hotkeyLog) << "Registered push-to-talk shortcut"
                      << m_config.sequence
                      << "on component"
                      << m_config.componentUnique
                      << "action"
                      << m_config.actionUnique;
    qCInfo(hotkeyLog) << "Assigned sequence:" << m_assignedSequence.toString(QKeySequence::PortableText);
    qCInfo(hotkeyLog) << "KGlobalAccel component active:" << KGlobalAccel::isComponentActive(m_config.componentUnique);
    return true;
}

void HotkeyManager::unregisterShortcut()
{
    if (!m_registered || m_action == nullptr) {
        return;
    }

    KGlobalAccel::self()->removeAllShortcuts(m_action);
    m_registered = false;
    m_shortcutActive = false;
    m_assignedSequence = QKeySequence();
}

bool HotkeyManager::invokeShortcut(QString *errorMessage)
{
    if (!m_registered) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Shortcut is not registered");
        }
        return false;
    }

    // Diagnostics can exercise the same press/release path without synthesizing a real
    // global key event through the desktop session.
    onGlobalShortcutActiveChanged(m_action, true);
    QTimer::singleShot(0, this, [this]() {
        onGlobalShortcutActiveChanged(m_action, false);
    });
    return true;
}

QJsonObject HotkeyManager::diagnostics() const
{
    QJsonObject object;
    object.insert(QStringLiteral("component_path"), QStringLiteral("/component/%1").arg(m_config.componentUnique));
    object.insert(QStringLiteral("configured_sequence"), m_config.sequence);
    object.insert(QStringLiteral("assigned_sequence"), sequenceToJson(m_assignedSequence));
    object.insert(QStringLiteral("assigned_sequence_text"), m_assignedSequence.toString(QKeySequence::PortableText));
    object.insert(QStringLiteral("pressed_events"), m_pressedEvents);
    object.insert(QStringLiteral("released_events"), m_releasedEvents);
    object.insert(QStringLiteral("registered"), m_registered);
    object.insert(QStringLiteral("component_active"), KGlobalAccel::isComponentActive(m_config.componentUnique));

    QJsonArray shortcutNames;
    if (m_registered) {
        shortcutNames.append(m_config.actionUnique);
    }
    object.insert(QStringLiteral("shortcut_names"), shortcutNames);
    return object;
}

void HotkeyManager::onGlobalShortcutActiveChanged(QAction *action, bool active)
{
    if (action != m_action || !m_registered || m_shortcutActive == active) {
        return;
    }

    m_shortcutActive = active;
    if (active) {
        ++m_pressedEvents;
        qCInfo(hotkeyLog) << "Observed shortcut press event #" << m_pressedEvents;
        emit shortcutPressed();
        return;
    }

    ++m_releasedEvents;
    qCInfo(hotkeyLog) << "Observed shortcut release event #" << m_releasedEvents;
    emit shortcutReleased();
}

bool HotkeyManager::parseSequence(QKeySequence *sequence, QString *errorMessage) const
{
    if (sequence == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No sequence output target was provided");
        }
        return false;
    }

    const QKeySequence parsed = parseKeySequence(m_config.sequence);
    if (parsed.count() == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not parse shortcut sequence: %1").arg(m_config.sequence);
        }
        return false;
    }

    *sequence = parsed;
    return true;
}
