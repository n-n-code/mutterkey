#include "app/applicationcommands.h"

#include "config.h"
#include "audio/audiorecorder.h"
#include "audio/recording.h"
#include "clipboardwriter.h"
#include "control/daemoncontrolserver.h"
#include "service.h"
#include "transcription/transcriptionengine.h"
#include "transcription/transcriptiontypes.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QObject>
#include <QString>
#include <QTextStream>
#include <QTimer>
#include <QtLogging>
#include <QtCore/qnamespace.h>

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
    const std::shared_ptr<const TranscriptionEngine> transcriptionEngine = createTranscriptionEngine(config.transcriber);
    MutterkeyService service(config, transcriptionEngine, QGuiApplication::clipboard());
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
    return QGuiApplication::exec();
}

int runOnce(QGuiApplication &app, const AppConfig &config, double seconds)
{
    AudioRecorder recorder(config.audio);
    const std::shared_ptr<const TranscriptionEngine> transcriptionEngine = createTranscriptionEngine(config.transcriber);
    std::unique_ptr<TranscriptionSession> transcriber = transcriptionEngine->createSession();
    ClipboardWriter clipboardWriter(QGuiApplication::clipboard());

    if (config.transcriber.warmupOnStart) {
        RuntimeError runtimeError;
        if (!transcriber->warmup(&runtimeError)) {
            qCCritical(appLog) << "Failed to warm up transcriber:" << runtimeError.message;
            return 1;
        }
    }

    QTimer::singleShot(0, &app, [&app, &recorder, transcriber = transcriber.get(), &clipboardWriter, seconds]() {
        QString errorMessage;
        if (!recorder.start(&errorMessage)) {
            qCCritical(appLog) << "Failed to start one-shot recording:" << errorMessage;
            QGuiApplication::exit(1);
            return;
        }

        qCInfo(appLog) << "Recording for" << seconds << "seconds";
        QTimer::singleShot(static_cast<int>(seconds * 1000), &app, [&app, &recorder, transcriber, &clipboardWriter]() {
            const Recording recording = recorder.stop();
            if (!recording.isValid()) {
                qCCritical(appLog) << "Recorder returned no audio";
                QGuiApplication::exit(1);
                return;
            }

            const TranscriptionResult result = transcriber->transcribe(recording);
            if (!result.success) {
                qCCritical(appLog) << "One-shot transcription failed:" << result.error.message;
                QGuiApplication::exit(1);
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
            QGuiApplication::exit(0);
        });
    });

    return QGuiApplication::exec();
}

int runDiagnose(QGuiApplication &app, const AppConfig &config, double seconds, bool invokeShortcut)
{
    const std::shared_ptr<const TranscriptionEngine> transcriptionEngine = createTranscriptionEngine(config.transcriber);
    MutterkeyService service(config, transcriptionEngine, QGuiApplication::clipboard());
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
        QGuiApplication::exit(0);
    });

    return QGuiApplication::exec();
}
