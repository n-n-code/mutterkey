#pragma once

#include "config.h"

#include <QLoggingCategory>

class QGuiApplication;

Q_DECLARE_LOGGING_CATEGORY(appLog)

/**
 * @file
 * @brief Runtime command helpers shared by the CLI entrypoint.
 */

/**
 * @brief Configures runtime log filtering for Mutterkey commands.
 * @param level Configured log level string.
 */
void configureLogging(const QString &level);

/**
 * @brief Runs the background daemon mode.
 * @param app GUI application object.
 * @param config Resolved runtime config snapshot.
 * @param configPath Config path associated with the current daemon session.
 * @return Process exit code.
 */
int runDaemon(QGuiApplication &app, const AppConfig &config, const QString &configPath);

/**
 * @brief Runs one-shot recording and transcription mode.
 * @param app GUI application object.
 * @param config Resolved runtime config snapshot.
 * @param seconds Recording duration in seconds.
 * @return Process exit code.
 */
int runOnce(QGuiApplication &app, const AppConfig &config, double seconds);

/**
 * @brief Runs temporary daemon diagnostics.
 * @param app GUI application object.
 * @param config Resolved runtime config snapshot.
 * @param seconds Diagnostic capture duration in seconds.
 * @param invokeShortcut Whether to trigger a synthetic shortcut invoke.
 * @return Process exit code.
 */
int runDiagnose(QGuiApplication &app, const AppConfig &config, double seconds, bool invokeShortcut);

/**
 * @brief Imports a legacy raw Whisper model into a native Mutterkey package.
 * @param sourcePath Source raw model file.
 * @param outputPath Optional package destination path.
 * @param packageIdOverride Optional package id override.
 * @return Process exit code.
 */
int runModelImport(const QString &sourcePath, const QString &outputPath, const QString &packageIdOverride);

/**
 * @brief Inspects a model artifact and prints metadata as JSON.
 * @param path Package directory, manifest path, or raw compatibility artifact.
 * @return Process exit code.
 */
int runModelInspect(const QString &path);
