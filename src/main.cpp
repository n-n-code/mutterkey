#include "app/applicationcommands.h"
#include "commanddispatch.h"
#include "config.h"

#include <algorithm>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTextStream>

#if defined(Q_OS_WIN)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

struct ConfigOverride {
    ConfigOverride(QString keyValue, QString valueValue)
        : key(std::move(keyValue))
        , value(std::move(valueValue))
    {
    }

    QString key;
    QString value;
};

void writeConfigHelp()
{
    QTextStream(stdout) << configHelpText();
}

void configureCommandLineParser(QCommandLineParser *parser)
{
    parser->setApplicationDescription(QStringLiteral("Push-to-talk local speech transcription for KDE Plasma"));
    parser->addHelpOption();
}

int exitWithError(const QString &message)
{
    QTextStream(stderr) << message << Qt::endl;
    qCCritical(appLog).noquote() << message;
    return 1;
}

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

bool isInteractiveTerminal()
{
#if defined(Q_OS_WIN)
    return _isatty(_fileno(stdin)) != 0 && _isatty(_fileno(stdout)) != 0;
#else
    return isatty(STDIN_FILENO) != 0 && isatty(STDOUT_FILENO) != 0;
#endif
}

bool promptForConfigValue(QStringView label,
                          const QString &defaultValue,
                          bool requireNonEmpty,
                          QString &valueOut,
                          QString *errorMessage)
{
    QTextStream input(stdin);
    QTextStream output(stdout);
    while (true) {
        output << label;
        if (!defaultValue.isEmpty()) {
            output << " [" << defaultValue << "]";
        }
        output << ": " << Qt::flush;

        const QString line = input.readLine();
        if (line.isNull()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Input closed during interactive setup");
            }
            return false;
        }

        const QString trimmedLine = line.trimmed();
        const QString resolvedValue = trimmedLine.isEmpty() ? defaultValue.trimmed() : trimmedLine;
        if (!requireNonEmpty || !resolvedValue.isEmpty()) {
            valueOut = resolvedValue;
            return true;
        }

        output << "A value is required." << Qt::endl;
    }
}

bool applyOverrides(AppConfig *config, const QList<ConfigOverride> &overrides, QString *errorMessage)
{
    return std::ranges::all_of(overrides, [config, errorMessage](const ConfigOverride &overrideValue) {
        if (!applyConfigValue(config, overrideValue.key, overrideValue.value, errorMessage)) {
            if (errorMessage != nullptr && !errorMessage->isEmpty()) {
                *errorMessage = QStringLiteral("Invalid value for %1: %2").arg(overrideValue.key, *errorMessage);
            }
            return false;
        }
        return true;
    });
}

bool collectConfigOverrides(const QCommandLineParser &parser,
                            const QCommandLineOption &logLevelOption,
                            const QCommandLineOption &modelPathOption,
                            const QCommandLineOption &shortcutOption,
                            const QCommandLineOption &languageOption,
                            const QCommandLineOption &threadsOption,
                            const QCommandLineOption &translateOption,
                            const QCommandLineOption &noTranslateOption,
                            const QCommandLineOption &warmupOption,
                            const QCommandLineOption &noWarmupOption,
                            QList<ConfigOverride> *overridesOut,
                            QString *errorMessage)
{
    if (overridesOut == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Internal error: missing overrides output target");
        }
        return false;
    }

    overridesOut->clear();
    if (parser.isSet(logLevelOption)) {
        overridesOut->append(ConfigOverride{QStringLiteral("log_level"), parser.value(logLevelOption)});
    }
    if (parser.isSet(modelPathOption)) {
        overridesOut->append(ConfigOverride{QStringLiteral("transcriber.model_path"), parser.value(modelPathOption)});
    }
    if (parser.isSet(shortcutOption)) {
        overridesOut->append(ConfigOverride{QStringLiteral("shortcut.sequence"), parser.value(shortcutOption)});
    }
    if (parser.isSet(languageOption)) {
        overridesOut->append(ConfigOverride{QStringLiteral("transcriber.language"), parser.value(languageOption)});
    }
    if (parser.isSet(threadsOption)) {
        overridesOut->append(ConfigOverride{QStringLiteral("transcriber.threads"), parser.value(threadsOption)});
    }

    if (parser.isSet(translateOption) && parser.isSet(noTranslateOption)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Use only one of --translate or --no-translate");
        }
        return false;
    }
    if (parser.isSet(translateOption)) {
        overridesOut->append(ConfigOverride{QStringLiteral("transcriber.translate"), QStringLiteral("true")});
    }
    if (parser.isSet(noTranslateOption)) {
        overridesOut->append(ConfigOverride{QStringLiteral("transcriber.translate"), QStringLiteral("false")});
    }

    if (parser.isSet(warmupOption) && parser.isSet(noWarmupOption)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Use only one of --warmup-on-start or --no-warmup-on-start");
        }
        return false;
    }
    if (parser.isSet(warmupOption)) {
        overridesOut->append(ConfigOverride{QStringLiteral("transcriber.warmup_on_start"), QStringLiteral("true")});
    }
    if (parser.isSet(noWarmupOption)) {
        overridesOut->append(ConfigOverride{QStringLiteral("transcriber.warmup_on_start"), QStringLiteral("false")});
    }

    return true;
}

bool loadConfigForMutation(const QString &configPath, AppConfig *configOut, QString *errorMessage)
{
    if (configOut == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Internal error: missing config output target");
        }
        return false;
    }

    if (!QFileInfo::exists(configPath)) {
        *configOut = defaultAppConfig();
        return true;
    }

    QString loadError;
    const AppConfig config = loadConfig(configPath, &loadError);
    if (!loadError.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = loadError;
        }
        return false;
    }

    *configOut = config;
    return true;
}

bool bootstrapConfig(const QString &configPath,
                     AppConfig *config,
                     bool promptForModelPath,
                     bool promptForShortcut,
                     QString *errorMessage)
{
    if (config == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Internal error: missing bootstrap config");
        }
        return false;
    }

    QTextStream output(stdout);
    output << "Creating Mutterkey config at " << configPath << Qt::endl;

    if (promptForModelPath) {
        QString modelPath;
        if (!promptForConfigValue(QStringLiteral("Whisper model path"),
                                  config->transcriber.modelPath,
                                  true,
                                  modelPath,
                                  errorMessage)) {
            return false;
        }
        if (!applyConfigValue(config, QStringLiteral("transcriber.model_path"), modelPath, errorMessage)) {
            return false;
        }
    }

    if (promptForShortcut) {
        QString shortcutSequence;
        if (!promptForConfigValue(QStringLiteral("Push-to-talk shortcut"),
                                  config->shortcut.sequence,
                                  true,
                                  shortcutSequence,
                                  errorMessage)) {
            return false;
        }
        if (!applyConfigValue(config, QStringLiteral("shortcut.sequence"), shortcutSequence, errorMessage)) {
            return false;
        }
    }

    if (!saveConfig(configPath, *config, errorMessage)) {
        return false;
    }

    output << "Saved config to " << configPath << Qt::endl;
    return true;
}

int runConfigInit(const QString &configPath,
                  const QList<ConfigOverride> &overrides,
                  bool interactive,
                  bool hasModelPathOverride)
{
    if (QFileInfo::exists(configPath)) {
        return exitWithError(QStringLiteral("Config file already exists: %1").arg(configPath));
    }

    AppConfig config = defaultAppConfig();
    QString errorMessage;
    if (!applyOverrides(&config, overrides, &errorMessage)) {
        return exitWithError(errorMessage);
    }

    if (interactive) {
        if (!bootstrapConfig(configPath,
                             &config,
                             !hasModelPathOverride,
                             true,
                             &errorMessage)) {
            return exitWithError(errorMessage);
        }
    } else {
        if (!hasModelPathOverride) {
            return exitWithError(QStringLiteral(
                "Missing config file and no terminal is available. Re-run `mutterkey config init --model-path /path/to/model.bin` from a shell."));
        }
        if (!saveConfig(configPath, config, &errorMessage)) {
            return exitWithError(errorMessage);
        }
        QTextStream(stdout) << "Saved config to " << configPath << Qt::endl;
    }

    return 0;
}

int runConfigSet(const QString &configPath, const QStringList &positionals)
{
    if (positionals.size() < 4) {
        return exitWithError(QStringLiteral("Usage: mutterkey config set <key> <value>"));
    }

    AppConfig config;
    QString errorMessage;
    if (!loadConfigForMutation(configPath, &config, &errorMessage)) {
        return exitWithError(errorMessage);
    }

    const QString key = positionals.at(2).trimmed();
    const QString &value = positionals.at(3);
    if (!applyConfigValue(&config, key, value, &errorMessage)) {
        return exitWithError(errorMessage);
    }

    if (!saveConfig(configPath, config, &errorMessage)) {
        return exitWithError(errorMessage);
    }

    QTextStream(stdout) << "Updated " << key << " in " << configPath << Qt::endl;
    return 0;
}

int runConfigCommand(const QStringList &arguments)
{
    QCommandLineParser parser;
    configureCommandLineParser(&parser);

    const QCommandLineOption configOption(QStringList{QStringLiteral("config")},
                                          QStringLiteral("Path to the JSON config file"),
                                          QStringLiteral("path"),
                                          defaultConfigPath());
    const QCommandLineOption logLevelOption(QStringList{QStringLiteral("log-level")},
                                            QStringLiteral("Override the configured log level"),
                                            QStringLiteral("level"));
    const QCommandLineOption modelPathOption(QStringList{QStringLiteral("model-path")},
                                             QStringLiteral("Override the configured Whisper model path"),
                                             QStringLiteral("path"));
    const QCommandLineOption shortcutOption(QStringList{QStringLiteral("shortcut")},
                                            QStringLiteral("Override the configured push-to-talk shortcut"),
                                            QStringLiteral("sequence"));
    const QCommandLineOption languageOption(QStringList{QStringLiteral("language")},
                                            QStringLiteral("Override the configured transcription language code or use auto-detect"),
                                            QStringLiteral("code|auto"));
    const QCommandLineOption threadsOption(QStringList{QStringLiteral("threads")},
                                           QStringLiteral("Override the configured transcription thread count"),
                                           QStringLiteral("count"));
    const QCommandLineOption translateOption(QStringList{QStringLiteral("translate")},
                                             QStringLiteral("Translate speech to English"));
    const QCommandLineOption noTranslateOption(QStringList{QStringLiteral("no-translate")},
                                               QStringLiteral("Disable translation to English"));
    const QCommandLineOption warmupOption(QStringList{QStringLiteral("warmup-on-start")},
                                          QStringLiteral("Warm up the transcriber during startup"));
    const QCommandLineOption noWarmupOption(QStringList{QStringLiteral("no-warmup-on-start")},
                                            QStringLiteral("Disable transcriber warmup during startup"));
    parser.addOption(configOption);
    parser.addOption(logLevelOption);
    parser.addOption(modelPathOption);
    parser.addOption(shortcutOption);
    parser.addOption(languageOption);
    parser.addOption(threadsOption);
    parser.addOption(translateOption);
    parser.addOption(noTranslateOption);
    parser.addOption(warmupOption);
    parser.addOption(noWarmupOption);
    parser.addPositionalArgument(QStringLiteral("command"), QStringLiteral("daemon, once, diagnose, or config"));
    parser.addPositionalArgument(QStringLiteral("extra"), QStringLiteral("Command-specific arguments"));
    parser.process(arguments);

    QList<ConfigOverride> overrides;
    QString overrideError;
    if (!collectConfigOverrides(parser,
                                logLevelOption,
                                modelPathOption,
                                shortcutOption,
                                languageOption,
                                threadsOption,
                                translateOption,
                                noTranslateOption,
                                warmupOption,
                                noWarmupOption,
                                &overrides,
                                &overrideError)) {
        return exitWithError(overrideError);
    }

    const QString configPath = parser.value(configOption);
    const bool interactive = isInteractiveTerminal();
    const bool hasModelPathOverride = parser.isSet(modelPathOption);
    const QStringList positional = parser.positionalArguments();
    const QString subcommand = positional.value(1);

    if (subcommand == QStringLiteral("init")) {
        return runConfigInit(configPath, overrides, interactive, hasModelPathOverride);
    }

    if (!overrides.isEmpty()) {
        return exitWithError(QStringLiteral("Runtime override flags are not supported with `config set`."));
    }

    if (subcommand == QStringLiteral("set")) {
        return runConfigSet(configPath, positional);
    }

    return exitWithError(QStringLiteral("Unknown config subcommand: %1").arg(subcommand));
}

} // namespace

int main(int argc, char *argv[])
{
    const QStringList arguments = rawArguments(std::span<char *const>(argv, argc));
    const int commandIndex = commandIndexFromArguments(arguments);
    if (shouldShowConfigHelp(arguments, commandIndex)) {
        writeConfigHelp();
        return 0;
    }
    if (commandIndex >= 0 && commandIndex < arguments.size() && arguments.at(commandIndex) == QStringLiteral("config")) {
        return runConfigCommand(arguments);
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setDesktopFileName(QStringLiteral("org.mutterkey.mutterkey"));
    QGuiApplication::setApplicationName(QStringLiteral("mutterkey"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Mutterkey"));

    QCommandLineParser parser;
    configureCommandLineParser(&parser);

    const QCommandLineOption configOption(QStringList{QStringLiteral("config")},
                                          QStringLiteral("Path to the JSON config file"),
                                          QStringLiteral("path"),
                                          defaultConfigPath());
    const QCommandLineOption logLevelOption(QStringList{QStringLiteral("log-level")},
                                            QStringLiteral("Override the configured log level"),
                                            QStringLiteral("level"));
    const QCommandLineOption modelPathOption(QStringList{QStringLiteral("model-path")},
                                             QStringLiteral("Override the configured Whisper model path"),
                                             QStringLiteral("path"));
    const QCommandLineOption shortcutOption(QStringList{QStringLiteral("shortcut")},
                                            QStringLiteral("Override the configured push-to-talk shortcut"),
                                            QStringLiteral("sequence"));
    const QCommandLineOption languageOption(QStringList{QStringLiteral("language")},
                                            QStringLiteral("Override the configured transcription language code or use auto-detect"),
                                            QStringLiteral("code|auto"));
    const QCommandLineOption threadsOption(QStringList{QStringLiteral("threads")},
                                           QStringLiteral("Override the configured transcription thread count"),
                                           QStringLiteral("count"));
    const QCommandLineOption translateOption(QStringList{QStringLiteral("translate")},
                                             QStringLiteral("Translate speech to English"));
    const QCommandLineOption noTranslateOption(QStringList{QStringLiteral("no-translate")},
                                               QStringLiteral("Disable translation to English"));
    const QCommandLineOption warmupOption(QStringList{QStringLiteral("warmup-on-start")},
                                          QStringLiteral("Warm up the transcriber during startup"));
    const QCommandLineOption noWarmupOption(QStringList{QStringLiteral("no-warmup-on-start")},
                                            QStringLiteral("Disable transcriber warmup during startup"));
    parser.addOption(configOption);
    parser.addOption(logLevelOption);
    parser.addOption(modelPathOption);
    parser.addOption(shortcutOption);
    parser.addOption(languageOption);
    parser.addOption(threadsOption);
    parser.addOption(translateOption);
    parser.addOption(noTranslateOption);
    parser.addOption(warmupOption);
    parser.addOption(noWarmupOption);
    parser.addPositionalArgument(QStringLiteral("command"), QStringLiteral("daemon, once, diagnose, or config"));
    parser.addPositionalArgument(QStringLiteral("extra"), QStringLiteral("Command-specific arguments"));
    parser.process(app);

    QList<ConfigOverride> overrides;
    QString overrideError;
    if (!collectConfigOverrides(parser,
                                logLevelOption,
                                modelPathOption,
                                shortcutOption,
                                languageOption,
                                threadsOption,
                                translateOption,
                                noTranslateOption,
                                warmupOption,
                                noWarmupOption,
                                &overrides,
                                &overrideError)) {
        return exitWithError(overrideError);
    }

    const QString configPath = parser.value(configOption);
    const bool interactive = isInteractiveTerminal();
    const bool hasModelPathOverride = parser.isSet(modelPathOption);

    const QStringList positional = parser.positionalArguments();
    const QString command = positional.isEmpty() ? QStringLiteral("daemon") : positional.first();

    if (!QFileInfo::exists(configPath)) {
        if (!interactive) {
            return exitWithError(
                QStringLiteral("Config file not found: %1\nRun `mutterkey config init` from a terminal to create it.").arg(configPath));
        }

        AppConfig initialConfig = defaultAppConfig();
        QString bootstrapError;
        if (!applyOverrides(&initialConfig, overrides, &bootstrapError)) {
            return exitWithError(bootstrapError);
        }
        if (!bootstrapConfig(configPath, &initialConfig, !hasModelPathOverride, true, &bootstrapError)) {
            return exitWithError(bootstrapError);
        }
    }

    // Config parsing is intentionally non-fatal so recovery paths remain reachable
    // for malformed or partially invalid files.
    QString configError;
    AppConfig config = loadConfig(configPath, &configError);
    if (!configError.isEmpty()) {
        qWarning().noquote() << configError;
    }
    if (!applyOverrides(&config, overrides, &overrideError)) {
        return exitWithError(overrideError);
    }
    configureLogging(config.logLevel);

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

    return runDaemon(app, config, configPath);
}
