#pragma once

#include "transcription/cpudecoderforward.h"
#include "transcription/transcriptiontypes.h"

#include <vector>

/**
 * @file
 * @brief Session-owned mutable state helpers for the native CPU runtime.
 */

/**
 * @brief Mutable audio-buffer and lifecycle state for one native CPU session.
 */
class CpuSessionState final
{
public:
    /**
     * @brief Appends one validated audio chunk to the buffered utterance state.
     * @param chunk Validated normalized audio chunk.
     */
    void appendChunk(const AudioChunk &chunk);

    /**
     * @brief Returns the buffered utterance samples.
     * @return Session-owned mono PCM sample buffer.
     */
    [[nodiscard]] const std::vector<float> &bufferedSamples() const;

    /**
     * @brief Returns whether the session has completed warmup.
     * @return `true` when warmup has succeeded.
     */
    [[nodiscard]] bool isWarmedUp() const;

    /**
     * @brief Returns whether cancellation has been requested.
     * @return `true` when cooperative cancellation has been requested.
     */
    [[nodiscard]] bool cancelRequested() const;

    /**
     * @brief Marks the session as warmed up and clears stale cancellation.
     */
    void markWarmedUp();

    /**
     * @brief Requests cooperative cancellation for the session.
     */
    void requestCancel();

    /**
     * @brief Clears buffered audio and cancellation state for the next utterance.
     */
    void resetForNextUtterance();

    /**
     * @brief Returns the mutable KV cache for the real decoder path.
     * @return Reference to the session-owned KV cache.
     */
    [[nodiscard]] CpuKVCache &kvCache();

    /**
     * @brief Allocates the KV cache for the given model configuration.
     * @param config Model hyperparameters.
     * @param maxDecoderTokens Maximum decoder tokens per utterance.
     */
    void allocateKVCache(const CpuModelConfig &config, int maxDecoderTokens);

private:
    std::vector<float> m_bufferedSamples;
    CpuKVCache m_kvCache;
    bool m_warmedUp = false;
    bool m_cancelRequested = false;
};
