#pragma once

#include <QClipboard>
#include <QObject>
#include <QString>

class KSystemClipboard;

/**
 * @file
 * @brief Clipboard integration with KDE-first and Qt fallback behavior.
 */

/**
 * @brief Copies transcription text to the clipboard.
 *
 * The writer prefers KDE's system clipboard integration when available and
 * falls back to the standard Qt clipboard API otherwise.
 */
class ClipboardWriter final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates a clipboard writer bound to the application's clipboard.
     * @param clipboard Non-owning pointer to the process clipboard instance.
     * @param parent Optional QObject parent.
     */
    explicit ClipboardWriter(QClipboard *clipboard, QObject *parent = nullptr);

    /**
     * @brief Copies text to the best available clipboard backend.
     * @param text Text to copy.
     * @return `true` when a backend accepted the copy request.
     */
    [[nodiscard]] bool copy(const QString &text);

    /**
     * @brief Reports the active backend name for diagnostics.
     * @return Human-readable backend identifier.
     */
    [[nodiscard]] QString backendName() const;

private:
    /**
     * @brief Copies text through KDE system clipboard support.
     * @param text Text to copy.
     * @return `true` when the KDE backend succeeded.
     */
    [[nodiscard]] bool copyWithSystemClipboard(const QString &text);

    /**
     * @brief Copies text through the Qt clipboard fallback.
     * @param text Text to copy.
     * @return `true` when the Qt backend succeeded.
     */
    [[nodiscard]] bool copyWithQtClipboard(const QString &text);

    /// Non-owning pointer to the QApplication clipboard object.
    QClipboard *m_clipboard = nullptr;
    /// Lazily used KDE clipboard helper when the platform supports it.
    KSystemClipboard *m_systemClipboard = nullptr;
};
