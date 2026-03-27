#pragma once

#include <span>
#include <QStringList>

/**
 * @brief Converts raw argv data into Qt strings.
 * @param arguments Raw argv span.
 * @return Raw command-line arguments as a QStringList.
 */
QStringList rawArguments(std::span<char *const> arguments);

/**
 * @brief Finds the first positional command after known global options.
 * @param arguments Raw command-line arguments.
 * @return Index of the command token, or `-1` when no command is present.
 */
int commandIndexFromArguments(const QStringList &arguments);

/**
 * @brief Returns whether the config command should print dedicated help.
 * @param arguments Raw command-line arguments.
 * @param commandIndex Index returned by commandIndexFromArguments().
 * @return `true` for bare `config` and `config --help` style invocations.
 */
bool shouldShowConfigHelp(const QStringList &arguments, int commandIndex);

/**
 * @brief Returns the dedicated help text for config subcommands.
 * @return Human-readable help text.
 */
QString configHelpText();

/**
 * @brief Returns whether the model command should print dedicated help.
 * @param arguments Raw command-line arguments.
 * @param commandIndex Index returned by commandIndexFromArguments().
 * @return `true` for bare `model` and `model --help` style invocations.
 */
bool shouldShowModelHelp(const QStringList &arguments, int commandIndex);

/**
 * @brief Returns the dedicated help text for model subcommands.
 * @return Human-readable help text.
 */
QString modelHelpText();
