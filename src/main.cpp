#include "audio/audiorecorder.h"
#include "clipboardwriter.h"
#include "config.h"
#include "service.h"
#include "transcription/whispercpptranscriber.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QTextStream>
#include <QTimer>

Q_LOGGING_CATEGORY(appLog, "mutterkey.app")

namespace {

bool parsePositiveSeconds(const QString &durationText,
                          double fallbackSeconds,
                          QStringView label,
                          double *secondsOut,
                          QString *errorMessage)
{
    if (secondsOut == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Internal error: missing duration output target");
        }
        return false;
    }

    const QString trimmedDurationText = durationText.trimmed();
    if (trimmedDurationText.isEmpty()) {
        *secondsOut = fallbackSeconds;
        return true;
    }

    bool ok = false;
    const double parsedSeconds = trimmedDurationText.toDouble(&ok);
    if (!ok || parsedSeconds <= 0.0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("%1 must be a positive number of seconds").arg(label);
        }
        return false;
    }

    *secondsOut = parsedSeconds;
    return true;
}

//
// logging
//

void configureLogging(const QString &level)
{
    qSetMessagePattern(QStringLiteral("%{time yyyy-MM-dd hh:mm:ss.zzz} %{if-debug}DEBUG%{endif}%{if-info}INFO%{endif}%{if-warning}WARNING%{endif}%{if-critical}ERROR%{endif}%{if-fatal}FATAL%{endif} %{category}: %{message}"));

    if (level.compare(QStringLiteral("DEBUG"), Qt::CaseInsensitive) == 0) {
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true"));
    } else {
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
    }
}

//
// command entrypoints
//

int runDaemon(QGuiApplication &app, const AppConfig &config)
{
    MutterkeyService service(config, app.clipboard());
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &service, &MutterkeyService::stop);

    QString errorMessage;
    if (!service.start(&errorMessage)) {
        qCCritical(appLog) << "Failed to start daemon:" << errorMessage;
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

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setDesktopFileName(QStringLiteral("org.mutterkey.mutterkey"));
    app.setApplicationName(QStringLiteral("mutterkey"));
    app.setApplicationDisplayName(QStringLiteral("Mutterkey"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Push-to-talk local speech transcription for KDE Plasma"));
    parser.addHelpOption();

    QCommandLineOption configOption(QStringList{QStringLiteral("config")},
                                    QStringLiteral("Path to the JSON config file"),
                                    QStringLiteral("path"),
                                    defaultConfigPath());
    QCommandLineOption logLevelOption(QStringList{QStringLiteral("log-level")},
                                      QStringLiteral("Override the configured log level"),
                                      QStringLiteral("level"));
    parser.addOption(configOption);
    parser.addOption(logLevelOption);
    parser.addPositionalArgument(QStringLiteral("command"), QStringLiteral("daemon, once, or diagnose"));
    parser.addPositionalArgument(QStringLiteral("extra"), QStringLiteral("Command-specific arguments"));
    parser.process(app);

    // Config parsing is intentionally non-fatal so the built-in defaults still let the
    // user reach --help, diagnose, or other recovery paths.
    QString configError;
    AppConfig config = loadConfig(parser.value(configOption), &configError);
    if (!configError.isEmpty()) {
        qWarning().noquote() << configError;
    }

    if (parser.isSet(logLevelOption)) {
        config.logLevel = parser.value(logLevelOption).toUpper();
    }
    configureLogging(config.logLevel);

    // The daemon path is the default product mode; the other commands are focused
    // validation and one-shot helpers around the same service/transcriber wiring.
    const QStringList positional = parser.positionalArguments();
    const QString command = positional.isEmpty() ? QStringLiteral("daemon") : positional.first();

    if (command == QStringLiteral("once")) {
        double seconds = 4.0;
        QString durationError;
        if (!parsePositiveSeconds(positional.value(1), seconds, QStringLiteral("once duration"), &seconds, &durationError)) {
            qCCritical(appLog) << durationError;
            return 1;
        }
        return runOnce(app, config, seconds);
    }

    if (command == QStringLiteral("diagnose")) {
        double seconds = 10.0;
        bool invokeShortcut = false;
        QString durationError;
        if (!parsePositiveSeconds(positional.value(1), seconds, QStringLiteral("diagnose duration"), &seconds, &durationError)) {
            qCCritical(appLog) << durationError;
            return 1;
        }
        if (positional.size() >= 3 && positional.at(2) == QStringLiteral("invoke")) {
            invokeShortcut = true;
        }
        return runDiagnose(app, config, seconds, invokeShortcut);
    }

    if (command != QStringLiteral("daemon")) {
        qCCritical(appLog) << "Unknown command:" << command;
        parser.showHelp(1);
    }

    return runDaemon(app, config);
}
