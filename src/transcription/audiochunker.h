#pragma once

#include "transcription/transcriptiontypes.h"

#include <QString>
#include <vector>

/**
 * @file
 * @brief Helpers for splitting normalized audio into deterministic stream chunks.
 */

/**
 * @brief Splits normalized utterance audio into fixed-size streaming chunks.
 */
class AudioChunker final
{
public:
    /**
     * @brief Converts normalized audio into ordered stream chunks.
     * @param audio Normalized utterance audio.
     * @param chunks Output destination for generated chunks.
     * @param errorMessage Optional output for validation failures.
     * @return `true` when chunking succeeded.
     */
    bool chunkAudio(const NormalizedAudio &audio, std::vector<AudioChunk> *chunks, QString *errorMessage = nullptr) const;
};
