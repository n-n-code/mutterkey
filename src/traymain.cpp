#include "tray/traystatuswindow.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("mutterkey-tray"));
    app.setApplicationDisplayName(QStringLiteral("Mutterkey Tray"));
    app.setQuitOnLastWindowClosed(false);

    TrayStatusWindow statusWindow;

    QIcon icon = QIcon::fromTheme(QStringLiteral("audio-input-microphone"));
    if (icon.isNull()) {
        icon = QApplication::windowIcon();
    }

    QSystemTrayIcon trayIcon(icon);
    auto *menu = new QMenu;

    QAction *showStatusAction = menu->addAction(QStringLiteral("Status"));
    QAction *refreshAction = menu->addAction(QStringLiteral("Refresh"));
    menu->addSeparator();
    QAction *quitAction = menu->addAction(QStringLiteral("Quit"));

    QObject::connect(showStatusAction, &QAction::triggered, &statusWindow, [&statusWindow]() {
        statusWindow.show();
        statusWindow.raise();
        statusWindow.activateWindow();
    });
    QObject::connect(refreshAction, &QAction::triggered, &statusWindow, &TrayStatusWindow::refresh);
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, &statusWindow, [&statusWindow](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            statusWindow.show();
            statusWindow.raise();
            statusWindow.activateWindow();
        }
    });
    QObject::connect(quitAction, &QAction::triggered, &app, &QCoreApplication::quit);

    trayIcon.setContextMenu(menu);
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        trayIcon.show();
    } else {
        statusWindow.show();
    }

    statusWindow.refresh();
    return app.exec();
}
