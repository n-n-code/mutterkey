#pragma once

#include "transcription/cpumelspectrogram.h"
#include "transcription/cputensor.h"
#include "transcription/transcriptiontypes.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

/**
 * @file
 * @brief Native model weight structures and MKCPUR3 format loader for the CPU runtime.
 */

/**
 * @brief Model hyperparameters for a Whisper-family model.
 */
struct CpuModelConfig {
    /// Number of mel input bands.
    int melBands = 80;
    /// Maximum encoder time steps after frontend padding/trimming.
    int audioContextSize = 1500;
    /// Encoder hidden width.
    int audioStateSize = 512;
    /// Encoder attention head count.
    int audioHeadCount = 8;
    /// Encoder transformer layer count.
    int audioLayerCount = 6;
    /// Maximum decoder token context size.
    int textContextSize = 448;
    /// Decoder hidden width.
    int textStateSize = 512;
    /// Decoder attention head count.
    int textHeadCount = 8;
    /// Decoder transformer layer count.
    int textLayerCount = 6;
    /// Token vocabulary size.
    int vocabularySize = 51864;
};

/**
 * @brief Weights for one encoder transformer layer.
 */
struct CpuEncoderLayerWeights {
    /// Self-attention layer norm.
    CpuTensor attnLnGamma;
    /// Self-attention layer norm bias.
    CpuTensor attnLnBeta;
    /// Self-attention Q/K/V/Out projections. Key has no bias in Whisper.
    CpuTensor queryWeight;
    /// Query projection bias.
    CpuTensor queryBias;
    /// Key projection weights.
    CpuTensor keyWeight;
    /// Value projection weights.
    CpuTensor valueWeight;
    /// Value projection bias.
    CpuTensor valueBias;
    /// Self-attention output projection weights.
    CpuTensor attnOutWeight;
    /// Self-attention output projection bias.
    CpuTensor attnOutBias;
    /// FFN layer norm.
    CpuTensor ffnLnGamma;
    /// FFN layer norm bias.
    CpuTensor ffnLnBeta;
    /// FFN projections (state -> 4*state, 4*state -> state).
    CpuTensor ffn1Weight;
    /// First FFN projection bias.
    CpuTensor ffn1Bias;
    /// Second FFN projection weights.
    CpuTensor ffn2Weight;
    /// Second FFN projection bias.
    CpuTensor ffn2Bias;
};

/**
 * @brief Complete encoder weight set.
 */
struct CpuEncoderWeights {
    /// First conv layer: (melBands, kernelSize=3) -> audioState.
    CpuTensor conv1Weight;
    /// First conv layer bias.
    CpuTensor conv1Bias;
    /// Second conv layer: (audioState, kernelSize=3, stride=2) -> audioState.
    CpuTensor conv2Weight;
    /// Second conv layer bias.
    CpuTensor conv2Bias;
    /// Sinusoidal positional embedding: (audioContextSize, audioState).
    CpuTensor positionalEmbedding;
    /// Per-layer transformer weights.
    std::vector<CpuEncoderLayerWeights> layers;
    /// Final layer norm.
    CpuTensor lnPostGamma;
    /// Final layer norm bias.
    CpuTensor lnPostBeta;
};

/**
 * @brief Weights for one decoder transformer layer.
 */
struct CpuDecoderLayerWeights {
    /// Self-attention layer norm.
    CpuTensor selfAttnLnGamma;
    /// Self-attention layer norm bias.
    CpuTensor selfAttnLnBeta;
    /// Self-attention Q/K/V/Out. Key has no bias in Whisper.
    CpuTensor selfQueryWeight;
    /// Self-attention query bias.
    CpuTensor selfQueryBias;
    /// Self-attention key weights.
    CpuTensor selfKeyWeight;
    /// Self-attention value weights.
    CpuTensor selfValueWeight;
    /// Self-attention value bias.
    CpuTensor selfValueBias;
    /// Self-attention output projection weights.
    CpuTensor selfAttnOutWeight;
    /// Self-attention output projection bias.
    CpuTensor selfAttnOutBias;
    /// Cross-attention layer norm.
    CpuTensor crossAttnLnGamma;
    /// Cross-attention layer norm bias.
    CpuTensor crossAttnLnBeta;
    /// Cross-attention Q/K/V/Out. Key has no bias in Whisper.
    CpuTensor crossQueryWeight;
    /// Cross-attention query bias.
    CpuTensor crossQueryBias;
    /// Cross-attention key weights.
    CpuTensor crossKeyWeight;
    /// Cross-attention value weights.
    CpuTensor crossValueWeight;
    /// Cross-attention value bias.
    CpuTensor crossValueBias;
    /// Cross-attention output projection weights.
    CpuTensor crossAttnOutWeight;
    /// Cross-attention output projection bias.
    CpuTensor crossAttnOutBias;
    /// FFN layer norm.
    CpuTensor ffnLnGamma;
    /// FFN layer norm bias.
    CpuTensor ffnLnBeta;
    /// FFN projections.
    CpuTensor ffn1Weight;
    /// First FFN projection bias.
    CpuTensor ffn1Bias;
    /// Second FFN projection weights.
    CpuTensor ffn2Weight;
    /// Second FFN projection bias.
    CpuTensor ffn2Bias;
};

/**
 * @brief Complete decoder weight set.
 */
struct CpuDecoderWeights {
    /// Token embedding: (vocabularySize, textState).
    CpuTensor tokenEmbedding;
    /// Learned positional embedding: (textContextSize, textState).
    CpuTensor positionalEmbedding;
    /// Per-layer transformer weights.
    std::vector<CpuDecoderLayerWeights> layers;
    /// Final layer norm.
    CpuTensor lnGamma;
    /// Final layer norm bias.
    CpuTensor lnBeta;
};

/**
 * @brief Complete loaded model weight set for a Whisper-family model.
 */
struct CpuWhisperModelWeights {
    /// Model hyperparameters.
    CpuModelConfig config;
    /// Mel filterbank loaded from the model file.
    CpuMelFilterBank melFilters;
    /// Encoder weights.
    CpuEncoderWeights encoder;
    /// Decoder weights.
    CpuDecoderWeights decoder;
};

/**
 * @brief Fixed header for the MKCPUR3 native tensor format.
 */
struct CpuTensorFileHeader {
    /// File magic: "MKCPUR3\0".
    std::array<char, 8> magic{};
    /// Format version (3).
    std::uint32_t version = 0;
    /// Number of named tensors in the file.
    std::uint32_t tensorCount = 0;
    /// Length of the JSON metadata section in bytes.
    std::uint32_t metadataBytes = 0;
};

/**
 * @brief One entry in the tensor directory.
 */
struct CpuTensorDirectoryEntry {
    /// Tensor name (e.g. "encoder.blocks.0.attn.query.weight").
    QString name;
    /// Number of dimensions (1 or 2).
    int nDims = 0;
    /// Dimension sizes.
    std::vector<int> dims;
    /// Data type: 0 = f32, 1 = f16.
    int dtype = 0;
    /// Byte offset into the data section.
    std::int64_t dataOffset = 0;
    /// Byte count for this tensor's data.
    std::int64_t dataSize = 0;
};

/**
 * @brief Loads a complete model weight set from a MKCPUR3 format file.
 * @param path Absolute path to the .mkweights file.
 * @param error Optional destination for structured load failures.
 * @return Loaded model weights on success.
 */
[[nodiscard]] std::shared_ptr<CpuWhisperModelWeights>
loadCpuWhisperModelWeights(const QString &path, RuntimeError *error = nullptr);
