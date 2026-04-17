#pragma once

#include "asr/nativecpu/cpudecoderforward.h"
#include "asr/nativecpu/cpumodelweights.h"
#include "asr/nativecpu/cputensor.h"
#include "asr/nativecpu/cputokenizer.h"

#include <vector>

/**
 * @file
 * @brief Greedy token generation loop for the native CPU Whisper runtime.
 *
 * Drives the autoregressive decoder to produce tokens one at a time using
 * argmax selection. Handles SOT/EOT framing, timestamp tokens, and no-speech
 * detection for the Whisper base.en model family.
 */

/**
 * @brief Configuration for the greedy search loop.
 */
struct CpuGreedySearchConfig {
    /// Start-of-transcript token id.
    int sotTokenId = 50257;
    /// End-of-transcript token id.
    int eotTokenId = 50256;
    /// No-timestamps token id.
    int noTimestampsTokenId = 50362;
    /// No-speech token id.
    int noSpeechTokenId = 50361;
    /// First timestamp token id.
    int timestampBeginId = 50363;
    /// Last timestamp token id (inclusive).
    int timestampEndId = 51863;
    /// Language token id (English).
    int languageTokenId = 50258;
    /// Transcribe task token id.
    int transcribeTokenId = 50358;
    /// Maximum number of decoder tokens before forced stop.
    int maxDecoderTokens = 224;
    /// Probability threshold for no-speech detection.
    float noSpeechThreshold = 0.6F;
    /// Prompt tokens fed after SOT. When empty, falls back to language + transcribe + no_timestamps.
    std::vector<int> initialPromptTokenIds;
    /// Token ids excluded from generated-token argmax selection.
    std::vector<int> suppressedTokenIds;
};

/**
 * @brief Result of one greedy search pass.
 */
struct CpuGreedySearchResult {
    /// Decoded tokens (lexical, timestamp, and control).
    std::vector<CpuDecodedToken> tokens;
    /// Whether speech was detected in the audio.
    bool isSpeech = false;
    /// Final assembled transcript text.
    QString transcript;
};

/**
 * @brief Runs the greedy token generation loop.
 *
 * Feeds SOT plus the packaged initial prompt sequence into the decoder, then
 * greedily selects the highest-probability token at each step until EOT is
 * produced or the maximum token count is reached.
 *
 * @param encoderOutput Encoder output of shape (encoderLen, audioState).
 * @param weights Complete model weights.
 * @param cache Mutable KV cache (must be allocated and reset before calling).
 * @param searchConfig Token ids and search parameters.
 * @return Greedy search result with tokens and transcript.
 */
[[nodiscard]] CpuGreedySearchResult runCpuGreedySearch(const CpuTensor &encoderOutput,
                                                        const CpuWhisperModelWeights &weights,
                                                        CpuKVCache *cache,
                                                        const CpuGreedySearchConfig &searchConfig);
