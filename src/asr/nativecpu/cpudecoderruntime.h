#pragma once

#include "asr/nativecpu/cpudecoderforward.h"
#include "asr/nativecpu/cpureferencemodel.h"
#include "asr/nativecpu/cputokenizer.h"
#include "asr/runtime/transcriptiontypes.h"

#include <optional>
#include <span>
#include <vector>

/**
 * @file
 * @brief Decoder-owned utterance execution helpers for the native CPU runtime.
 */

/**
 * @brief Immutable request for one native CPU utterance decode pass.
 */
struct CpuDecodeRequest {
    /// Buffered mono PCM samples for the utterance.
    std::span<const float> samples;
    /// Loaded native CPU model payload used for the decode pass.
    const CpuReferenceModelData *model = nullptr;
    /// Decoder execution metadata for tokenizer and timestamp interpretation.
    const CpuReferenceExecutionMetadata *execution = nullptr;
    /// Mutable KV cache for the real decoder path (nullptr for scaffold paths).
    CpuKVCache *kvCache = nullptr;
    /// Expected sample rate for timestamp synthesis.
    int sampleRate = 16000;
    /// Optional generated-token cap for bounded validation/benchmark passes; 0 keeps the runtime default.
    int maxDecoderTokens = 0;
    /// Optional mel-frame cap for short-fixture validation/benchmark passes; 0 keeps the runtime default.
    int maxMelFrames = 0;
};

/**
 * @brief Decoder-owned result for one native CPU utterance decode pass.
 */
struct CpuDecodeResult {
    /// Final transcript chosen by the decoder.
    QString transcript;
    /// Token ids/text emitted by the current tokenizer contract.
    std::vector<CpuDecodedToken> tokens;
    /// Final transcript event emitted for the utterance.
    TranscriptEvent event;
};

/**
 * @brief Runs one complete native CPU decode pass for a buffered utterance.
 * @param request Decoder request with samples and a loaded model payload.
 * @return Final decode result when the utterance matched, otherwise `nullopt`.
 */
[[nodiscard]] std::optional<CpuDecodeResult> runCpuDecodePass(const CpuDecodeRequest &request);
