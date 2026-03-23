#pragma once

#include <QObject>
#include <QString>

class QClipboard;
class KSystemClipboard;

class ClipboardWriter final : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardWriter(QClipboard *clipboard, QObject *parent = nullptr);

    [[nodiscard]] bool copy(const QString &text);
    [[nodiscard]] QString backendName() const;

private:
    [[nodiscard]] bool copyWithSystemClipboard(const QString &text);
    [[nodiscard]] bool copyWithQtClipboard(const QString &text);

    QClipboard *m_clipboard = nullptr;
    KSystemClipboard *m_systemClipboard = nullptr;
};
