#pragma once

#include "audio/recording.h"
#include "asr/runtime/transcriptiontypes.h"

#include <QString>

/**
 * @file
 * @brief Conversion from captured Qt audio to Whisper-ready normalized samples.
 */

/**
 * @brief Converts captured recordings into mono 16 kHz float samples.
 */
class RecordingNormalizer final
{
public:
    /**
     * @brief Converts a captured recording into runtime input audio.
     * @param recording Source recording and its original device format.
     * @param normalizedAudio Output location for normalized samples.
     * @param errorMessage Optional output for conversion failures.
     * @return `true` when normalization succeeded.
     */
    bool normalizeForRuntime(const Recording &recording, NormalizedAudio *normalizedAudio, QString *errorMessage = nullptr) const;
};
