#pragma once

#include "asr/nativecpu/cpumodelweights.h"
#include "asr/nativecpu/cputensor.h"

#include <vector>

/**
 * @file
 * @brief Decoder forward pass with KV cache for the native CPU Whisper runtime.
 *
 * Implements incremental autoregressive decoding: each call to decoderStep()
 * processes one new token, appending to the KV cache. Cross-attention KV is
 * computed once from the encoder output and reused for all decoder steps.
 */

/**
 * @brief Per-layer KV cache state.
 */
struct CpuKVCacheLayer {
    /// Self-attention key cache: (currentPos, dModel).
    CpuTensor selfKey;
    /// Self-attention value cache: (currentPos, dModel).
    CpuTensor selfValue;
    /// Cross-attention key: (encoderLen, dModel). Computed once.
    CpuTensor crossKey;
    /// Cross-attention value: (encoderLen, dModel). Computed once.
    CpuTensor crossValue;
};

/**
 * @brief Complete KV cache state for the decoder.
 */
struct CpuKVCache {
    /// Per-layer cache entries.
    std::vector<CpuKVCacheLayer> layers;
    /// Number of tokens decoded so far (rows used in self-attention KV).
    int currentPosition = 0;
    /// Whether cross-attention KV has been computed.
    bool crossAttentionReady = false;

    /**
     * @brief Allocates cache buffers for the given model configuration.
     * @param config Model hyperparameters.
     * @param maxDecoderTokens Maximum number of decoder tokens to cache.
     */
    void allocate(const CpuModelConfig &config, int maxDecoderTokens);

    /**
     * @brief Resets cache state for a new utterance without reallocating.
     */
    void reset();
};

/**
 * @brief Initializes the cross-attention KV cache from encoder output.
 *
 * Must be called once before any decoder steps. Projects the encoder output
 * through each layer's cross-attention K and V weights and stores the results.
 *
 * @param encoderOutput Encoder output of shape (encoderLen, audioState).
 * @param weights Decoder weights.
 * @param config Model hyperparameters.
 * @param cache KV cache to populate.
 */
void initializeCrossAttentionCache(const CpuTensor &encoderOutput,
                                    const CpuDecoderWeights &weights,
                                    const CpuModelConfig &config,
                                    CpuKVCache *cache);

/**
 * @brief Runs one incremental decoder step for a single token.
 *
 * Processes the token through embedding, positional encoding, masked
 * self-attention (using and updating the KV cache), cross-attention
 * (using the pre-computed encoder KV), FFN, and final layer norm.
 * Returns logits over the vocabulary.
 *
 * @param tokenId Input token id for this step.
 * @param weights Decoder weights.
 * @param config Model hyperparameters.
 * @param cache Mutable KV cache (updated in place).
 * @return Logits tensor of shape (1, vocabularySize).
 */
[[nodiscard]] CpuTensor decoderStep(int tokenId,
                                     const CpuDecoderWeights &weights,
                                     const CpuModelConfig &config,
                                     CpuKVCache *cache);
