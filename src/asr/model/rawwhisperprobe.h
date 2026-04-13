#pragma once

#include "asr/runtime/transcriptiontypes.h"

#include <optional>

/**
 * @file
 * @brief Lightweight app-owned parser for legacy raw whisper.cpp model files.
 */

/**
 * @brief Lightweight parser for legacy raw whisper.cpp model files.
 */
class RawWhisperProbe final
{
public:
    /**
     * @brief Reads header-level metadata from a legacy raw whisper.cpp model file.
     * @param path Raw `.bin` model path.
     * @param error Optional destination for parse failures.
     * @return Extracted product-owned model metadata on success.
     */
    [[nodiscard]] static std::optional<ModelMetadata> inspectFile(const QString &path, RuntimeError *error = nullptr);
};
