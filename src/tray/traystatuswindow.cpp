#include "tray/traystatuswindow.h"

#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QString prettyJson(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

} // namespace

TrayStatusWindow::TrayStatusWindow(DaemonControlSession *client, QWidget *parent)
    : QWidget(parent)
    , m_session(client != nullptr ? client : &m_ownedSession)
{
    setWindowTitle(QStringLiteral("Mutterkey Status"));
    resize(720, 480);

    auto *layout = new QVBoxLayout(this);
    auto *formLayout = new QFormLayout;

    m_connectionValue = new QLabel(this);
    m_connectionValue->setObjectName(QStringLiteral("connectionValue"));
    m_configPathValue = new QLabel(this);
    m_configPathValue->setObjectName(QStringLiteral("configPathValue"));
    m_shortcutValue = new QLabel(this);
    m_shortcutValue->setObjectName(QStringLiteral("shortcutValue"));
    m_modelPathValue = new QLabel(this);
    m_modelPathValue->setObjectName(QStringLiteral("modelPathValue"));

    formLayout->addRow(QStringLiteral("Connection"), m_connectionValue);
    formLayout->addRow(QStringLiteral("Config path"), m_configPathValue);
    formLayout->addRow(QStringLiteral("Shortcut"), m_shortcutValue);
    formLayout->addRow(QStringLiteral("Model path"), m_modelPathValue);

    m_statusJsonView = new QPlainTextEdit(this);
    m_statusJsonView->setObjectName(QStringLiteral("statusJsonView"));
    m_statusJsonView->setReadOnly(true);

    auto *refreshButton = new QPushButton(QStringLiteral("Refresh"), this);
    connect(refreshButton, &QPushButton::clicked, this, &TrayStatusWindow::refresh);

    layout->addLayout(formLayout);
    layout->addWidget(m_statusJsonView, 1);
    layout->addWidget(refreshButton);

    refresh();
}

void TrayStatusWindow::refresh()
{
    const DaemonStatusResult statusResult = m_session->fetchStatus(1500);
    if (!statusResult.success) {
        setOfflineState(statusResult.errorMessage);
        return;
    }

    const DaemonConfigResult configResult = m_session->fetchConfig(1500);
    if (!configResult.success) {
        setOfflineState(configResult.errorMessage);
        return;
    }

    m_connectionValue->setText(QStringLiteral("Connected"));
    m_configPathValue->setText(statusResult.snapshot.configPath);
    m_shortcutValue->setText(configResult.snapshot.config.shortcut.sequence);
    m_modelPathValue->setText(configResult.snapshot.config.transcriber.modelPath);
    m_statusJsonView->setPlainText(prettyJson(daemonStatusSnapshotToJsonObject(statusResult.snapshot)));
}

void TrayStatusWindow::setOfflineState(const QString &message)
{
    m_connectionValue->setText(QStringLiteral("Daemon unavailable"));
    m_configPathValue->setText(QStringLiteral("-"));
    m_shortcutValue->setText(QStringLiteral("-"));
    m_modelPathValue->setText(QStringLiteral("-"));

    QJsonObject object;
    object.insert(QStringLiteral("error"), message);
    object.insert(QStringLiteral("hint"), QStringLiteral("Start `mutterkey daemon` and refresh the tray window."));
    m_statusJsonView->setPlainText(prettyJson(object));
}
