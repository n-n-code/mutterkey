#include "clipboardwriter.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMimeData>
#include <ksystemclipboard.h>

namespace {

Q_LOGGING_CATEGORY(clipboardLog, "mutterkey.clipboard")

} // namespace

QString clipboardBackendName(bool hasSystemClipboard, bool hasQtClipboard)
{
    if (hasSystemClipboard) {
        return QStringLiteral("KSystemClipboard");
    }

    if (hasQtClipboard) {
        return QStringLiteral("Qt QClipboard");
    }

    return QStringLiteral("unavailable");
}

bool clipboardRoundTripSucceeded(const QString &requestedText, const QString &actualText)
{
    return actualText == requestedText;
}

ClipboardWriter::ClipboardWriter(QClipboard *clipboard, QObject *parent)
    : QObject(parent)
    , m_clipboard(clipboard)
    , m_systemClipboard(KSystemClipboard::instance())
{
}

bool ClipboardWriter::copy(const QString &text)
{
    if (text.isEmpty()) {
        return false;
    }

    // Prefer the KDE-aware clipboard path when available so background-service writes
    // integrate with the desktop session more reliably than the plain Qt fallback.
    if (m_systemClipboard != nullptr) {
        return copyWithSystemClipboard(text);
    }

    return copyWithQtClipboard(text);
}

QString ClipboardWriter::backendName() const
{
    return clipboardBackendName(m_systemClipboard != nullptr, m_clipboard != nullptr);
}

bool ClipboardWriter::copyWithSystemClipboard(const QString &text)
{
    auto *clipboardData = new QMimeData;
    clipboardData->setText(text);
    m_systemClipboard->setMimeData(clipboardData, QClipboard::Clipboard);

    if (m_clipboard != nullptr && m_clipboard->supportsSelection()) {
        auto *selectionData = new QMimeData;
        selectionData->setText(text);
        m_systemClipboard->setMimeData(selectionData, QClipboard::Selection);
    }

    const QString clipboardText = m_systemClipboard->text(QClipboard::Clipboard);
    const bool copied = clipboardRoundTripSucceeded(text, clipboardText);
    if (copied) {
        qCInfo(clipboardLog) << "Copied" << text.size() << "characters to the clipboard using" << backendName();
    } else {
        qCWarning(clipboardLog) << "System clipboard write did not round-trip via"
                                << backendName()
                                << ". Requested"
                                << text.size()
                                << "characters but read back"
                                << clipboardText.size();
    }

    return copied;
}

bool ClipboardWriter::copyWithQtClipboard(const QString &text)
{
    if (m_clipboard == nullptr) {
        return false;
    }

    m_clipboard->setText(text, QClipboard::Clipboard);
    if (m_clipboard->supportsSelection()) {
        m_clipboard->setText(text, QClipboard::Selection);
    }

    QGuiApplication::processEvents();

    const QString clipboardText = m_clipboard->text(QClipboard::Clipboard);
    const bool copied = clipboardRoundTripSucceeded(text, clipboardText);
    if (copied) {
        qCInfo(clipboardLog) << "Updated local clipboard state with"
                            << text.size()
                            << "characters using"
                            << backendName();
    } else {
        qCWarning(clipboardLog) << "Qt clipboard write did not round-trip. Requested"
                                << text.size()
                                << "characters but read back"
                                << clipboardText.size();
    }

    return copied;
}
