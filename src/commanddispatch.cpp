#include "commanddispatch.h"

#include "config.h"

#include <QString>
#include <QTextStream>

namespace {

bool optionConsumesValue(const QString &argument)
{
    static const QStringList kOptionsWithValues{
        QStringLiteral("--config"),
        QStringLiteral("--log-level"),
        QStringLiteral("--model-path"),
        QStringLiteral("--shortcut"),
        QStringLiteral("--language"),
        QStringLiteral("--threads"),
    };

    return kOptionsWithValues.contains(argument);
}

bool isHelpArgument(const QString &argument)
{
    return argument == QStringLiteral("--help") || argument == QStringLiteral("-h") || argument == QStringLiteral("--help-all");
}

} // namespace

QStringList rawArguments(std::span<char *const> arguments)
{
    QStringList parsedArguments;
    parsedArguments.reserve(static_cast<qsizetype>(arguments.size()));
    for (const char *argument : arguments) {
        parsedArguments.append(QString::fromLocal8Bit(argument));
    }
    return parsedArguments;
}

int commandIndexFromArguments(const QStringList &arguments)
{
    for (int index = 1; index < arguments.size(); ++index) {
        const QString &argument = arguments.at(index);
        if (argument == QStringLiteral("--")) {
            return index + 1 < arguments.size() ? index + 1 : -1;
        }

        if (optionConsumesValue(argument)) {
            ++index;
            continue;
        }

        if (argument.startsWith(QLatin1String("--")) && argument.contains(QLatin1Char('='))) {
            continue;
        }

        if (argument.startsWith(QLatin1Char('-'))) {
            continue;
        }

        return index;
    }

    return -1;
}

bool shouldShowConfigHelp(const QStringList &arguments, int commandIndex)
{
    if (commandIndex < 0 || commandIndex >= arguments.size() || arguments.at(commandIndex) != QStringLiteral("config")) {
        return false;
    }

    if (commandIndex == arguments.size() - 1) {
        return true;
    }

    for (int index = commandIndex + 1; index < arguments.size(); ++index) {
        if (isHelpArgument(arguments.at(index))) {
            return true;
        }
    }

    return false;
}

QString configHelpText()
{
    QString helpText;
    QTextStream output(&helpText);
    output << "Usage: mutterkey [options] config <subcommand> [args]" << Qt::endl;
    output << Qt::endl;
    output << "Configuration subcommands:" << Qt::endl;
    output << "  init                Create the config file, prompting on a terminal when needed" << Qt::endl;
    output << "  set <key> <value>   Persist one config value into the config file" << Qt::endl;
    output << Qt::endl;
    output << "Config options:" << Qt::endl;
    output << "  --config <path>       Path to the JSON config file" << Qt::endl;
    output << "  --model-path <path>   Set transcriber.model_path during `config init`" << Qt::endl;
    output << "  --shortcut <sequence> Set shortcut.sequence during `config init`" << Qt::endl;
    output << "  --language <code|auto> Set transcriber.language during `config init`" << Qt::endl;
    output << "  --threads <count>     Set transcriber.threads during `config init`" << Qt::endl;
    output << "  --translate           Set transcriber.translate=true during `config init`" << Qt::endl;
    output << "  --no-translate        Set transcriber.translate=false during `config init`" << Qt::endl;
    output << "  --warmup-on-start     Set transcriber.warmup_on_start=true during `config init`" << Qt::endl;
    output << "  --no-warmup-on-start  Set transcriber.warmup_on_start=false during `config init`" << Qt::endl;
    output << "  --log-level <level>   Set log_level during `config init`" << Qt::endl;
    output << Qt::endl;
    output << "Supported keys for `config set`:" << Qt::endl;
    for (const QString &key : supportedConfigKeys()) {
        output << "  " << key << Qt::endl;
    }
    return helpText;
}
