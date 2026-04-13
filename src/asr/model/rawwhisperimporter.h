#pragma once

#include "asr/model/modelpackage.h"
#include "asr/runtime/transcriptiontypes.h"

#include <optional>

/**
 * @file
 * @brief Import helpers that convert legacy raw Whisper model files into native packages.
 */

/**
 * @brief Request parameters for importing a raw Whisper artifact into a native package.
 */
struct RawWhisperImportRequest {
    /// Destination package path or parent directory. Empty uses the default models root.
    QString outputPath;
    /// Optional package id override.
    QString packageIdOverride;
};

/**
 * @brief Importer that converts legacy raw Whisper files into native packages.
 */
class RawWhisperImporter final
{
public:
    /**
     * @brief Imports a raw whisper.cpp-compatible `.bin` file into a native package.
     * @param sourcePath Source raw Whisper model path.
     * @param request Output-path and package-id overrides for the import.
     * @param error Optional destination for import failures.
     * @return Validated native package on success.
     */
    [[nodiscard]] static std::optional<ValidatedModelPackage>
    importFile(const QString &sourcePath, const RawWhisperImportRequest &request = {}, RuntimeError *error = nullptr);
};
