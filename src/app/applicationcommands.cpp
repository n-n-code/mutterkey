#include "app/applicationcommands.h"

#include "audio/audiorecorder.h"
#include "clipboardwriter.h"
#include "control/daemoncontrolserver.h"
#include "service.h"
#include "transcription/whispercpptranscriber.h"

#include <QGuiApplication>
#include <QJsonDocument>
#include <QTextStream>
#include <QTimer>

Q_LOGGING_CATEGORY(appLog, "mutterkey.app")

void configureLogging(const QString &level)
{
    qSetMessagePattern(QStringLiteral("%{time yyyy-MM-dd hh:mm:ss.zzz} %{if-debug}DEBUG%{endif}%{if-info}INFO%{endif}%{if-warning}WARNING%{endif}%{if-critical}ERROR%{endif}%{if-fatal}FATAL%{endif} %{category}: %{message}"));

    if (level.compare(QStringLiteral("DEBUG"), Qt::CaseInsensitive) == 0) {
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true"));
    } else {
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
    }
}

int runDaemon(QGuiApplication &app, const AppConfig &config, const QString &configPath)
{
    MutterkeyService service(config, app.clipboard());
    DaemonControlServer controlServer(configPath, config, &service);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &service, &MutterkeyService::stop);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &controlServer, &DaemonControlServer::stop);

    QString errorMessage;
    if (!service.start(&errorMessage)) {
        qCCritical(appLog) << "Failed to start daemon:" << errorMessage;
        return 1;
    }
    if (!controlServer.start(&errorMessage)) {
        qCCritical(appLog) << "Failed to start daemon control server:" << errorMessage;
        service.stop();
        return 1;
    }

    qCInfo(appLog) << "Mutterkey daemon running. Hold" << config.shortcut.sequence << "to talk.";
    return app.exec();
}

int runOnce(QGuiApplication &app, const AppConfig &config, double seconds)
{
    AudioRecorder recorder(config.audio);
    WhisperCppTranscriber transcriber(config.transcriber);
    ClipboardWriter clipboardWriter(app.clipboard());

    if (config.transcriber.warmupOnStart) {
        QString warmupError;
        if (!transcriber.warmup(&warmupError)) {
            qCCritical(appLog) << "Failed to warm up transcriber:" << warmupError;
            return 1;
        }
    }

    QTimer::singleShot(0, &app, [&app, &recorder, &transcriber, &clipboardWriter, seconds]() {
        QString errorMessage;
        if (!recorder.start(&errorMessage)) {
            qCCritical(appLog) << "Failed to start one-shot recording:" << errorMessage;
            app.exit(1);
            return;
        }

        qCInfo(appLog) << "Recording for" << seconds << "seconds";
        QTimer::singleShot(static_cast<int>(seconds * 1000), &app, [&app, &recorder, &transcriber, &clipboardWriter]() {
            const Recording recording = recorder.stop();
            if (!recording.isValid()) {
                qCCritical(appLog) << "Recorder returned no audio";
                app.exit(1);
                return;
            }

            const TranscriptionResult result = transcriber.transcribe(recording);
            if (!result.success) {
                qCCritical(appLog) << "One-shot transcription failed:" << result.error;
                app.exit(1);
                return;
            }

            if (!result.text.trimmed().isEmpty()) {
                const QString trimmedText = result.text.trimmed();
                if (!clipboardWriter.copy(trimmedText)) {
                    qCWarning(appLog) << "Clipboard update appears to have failed";
                }
                QTextStream(stdout) << trimmedText << Qt::endl;
            } else {
                qCInfo(appLog) << "No speech detected";
            }
            app.exit(0);
        });
    });

    return app.exec();
}

int runDiagnose(QGuiApplication &app, const AppConfig &config, double seconds, bool invokeShortcut)
{
    MutterkeyService service(config, app.clipboard());
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &service, &MutterkeyService::stop);

    QString errorMessage;
    if (!service.start(&errorMessage)) {
        qCCritical(appLog) << "Diagnostic startup failed:" << errorMessage;
        return 1;
    }

    qCInfo(appLog) << "Diagnostic mode active for" << seconds << "seconds. Press the configured shortcut now.";
    if (invokeShortcut) {
        QTimer::singleShot(750, &app, [&service]() {
            QString invokeError;
            if (!service.invokeShortcut(&invokeError)) {
                qCWarning(appLog) << "Diagnostic shortcut invoke failed:" << invokeError;
            } else {
                qCInfo(appLog) << "Invoked the registered shortcut through KGlobalAccel";
            }
        });
    }

    QTimer::singleShot(static_cast<int>(seconds * 1000), &app, [&app, &service]() {
        QTextStream(stdout) << QJsonDocument(service.diagnostics()).toJson(QJsonDocument::Indented);
        app.exit(0);
    });

    return app.exec();
}
