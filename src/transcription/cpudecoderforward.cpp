#include "transcription/cpudecoderforward.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <ranges>

namespace {

struct DecoderSelfAttentionRequest {
    const CpuTensor &x;
    const CpuDecoderLayerWeights &layerWeights;
    CpuKVCacheLayer *layerCache = nullptr;
    int position = 0;
    int nHead = 0;
};

struct DecoderCrossAttentionRequest {
    const CpuTensor &x;
    const CpuDecoderLayerWeights &layerWeights;
    const CpuKVCacheLayer &layerCache;
    int nHead = 0;
};
} // namespace

void CpuKVCache::allocate(const CpuModelConfig &config, int maxDecoderTokens)
{
    const int nLayers = config.textLayerCount;
    const int dModel = config.textStateSize;
    layers.resize(static_cast<std::size_t>(nLayers));
    for (CpuKVCacheLayer &layer : layers) {
        layer.selfKey = CpuTensor(maxDecoderTokens, dModel);
        layer.selfValue = CpuTensor(maxDecoderTokens, dModel);
        // Cross-attention KV is allocated when initialized from encoder output.
    }
    currentPosition = 0;
    crossAttentionReady = false;
}

void CpuKVCache::reset()
{
    for (CpuKVCacheLayer &layer : layers) {
        std::ranges::fill(layer.selfKey.data, 0.0F);
        std::ranges::fill(layer.selfValue.data, 0.0F);
    }
    currentPosition = 0;
    crossAttentionReady = false;
}

void initializeCrossAttentionCache(const CpuTensor &encoderOutput,
                                    const CpuDecoderWeights &weights,
                                    const CpuModelConfig &config,
                                    CpuKVCache *cache)
{
    for (int i = 0; i < config.textLayerCount; ++i) {
        const CpuDecoderLayerWeights &layerWeights = weights.layers.at(static_cast<std::size_t>(i));
        CpuKVCacheLayer &layerCache = cache->layers.at(static_cast<std::size_t>(i));

        // Project encoder output through cross-attention K and V weights.
        layerCache.crossKey = matmulTransposed(encoderOutput, layerWeights.crossKeyWeight);
        layerCache.crossValue = matmulTransposed(encoderOutput, layerWeights.crossValueWeight);
        addBiasInPlace(layerCache.crossValue, layerWeights.crossValueBias);
        // Cross-attention key has no bias in Whisper.
    }
    cache->crossAttentionReady = true;
}

namespace {

/**
 * @brief Masked self-attention for one incremental decoder step.
 *
 * Processes the new token's Q against all cached K/V up to the current position.
 * Updates the KV cache with the new token's projections.
 */
CpuTensor decoderSelfAttention(const DecoderSelfAttentionRequest &request)
{
    const int dModel = request.x.cols;
    const int dHead = dModel / request.nHead;
    const int kvLen = request.position + 1;

    // Project Q for the new token.
    CpuTensor q = matmulTransposed(request.x, request.layerWeights.selfQueryWeight);
    addBiasInPlace(q, request.layerWeights.selfQueryBias);

    // Project K and V for the new token and store in cache.
    const CpuTensor newK = matmulTransposed(request.x, request.layerWeights.selfKeyWeight);
    CpuTensor newV = matmulTransposed(request.x, request.layerWeights.selfValueWeight);
    addBiasInPlace(newV, request.layerWeights.selfValueBias);

    // Write new K/V row into cache at current position.
    std::memcpy(request.layerCache->selfKey.rowPtr(request.position), newK.data.data(),
                static_cast<std::size_t>(dModel) * sizeof(float));
    std::memcpy(request.layerCache->selfValue.rowPtr(request.position), newV.data.data(),
                static_cast<std::size_t>(dModel) * sizeof(float));

    // Per-head attention over the cached KV.
    const float scale = 1.0F / std::sqrt(static_cast<float>(dHead));
    CpuTensor output(1, dModel);

    for (int h = 0; h < request.nHead; ++h) {
        const int colOffset = h * dHead;
        std::vector<float> scores(static_cast<std::size_t>(kvLen), 0.0F);
        for (int kv = 0; kv < kvLen; ++kv) {
            float dot = 0.0F;
            for (int d = 0; d < dHead; ++d) {
                dot += q.at(0, colOffset + d) * request.layerCache->selfKey.at(kv, colOffset + d);
            }
            scores.at(static_cast<std::size_t>(kv)) = dot * scale;
        }

        // Softmax over kvLen scores.
        float maxScore = -std::numeric_limits<float>::infinity();
        for (int kv = 0; kv < kvLen; ++kv) {
            maxScore = std::max(maxScore, scores.at(static_cast<std::size_t>(kv)));
        }
        float sumExp = 0.0F;
        for (int kv = 0; kv < kvLen; ++kv) {
            const auto kvIndex = static_cast<std::size_t>(kv);
            scores.at(kvIndex) = std::exp(scores.at(kvIndex) - maxScore);
            sumExp += scores.at(kvIndex);
        }
        if (sumExp > 0.0F) {
            const float invSum = 1.0F / sumExp;
            for (int kv = 0; kv < kvLen; ++kv) {
                scores.at(static_cast<std::size_t>(kv)) *= invSum;
            }
        }

        // Weighted sum of V.
        for (int d = 0; d < dHead; ++d) {
            output.at(0, colOffset + d) = 0.0F;
        }
        for (int kv = 0; kv < kvLen; ++kv) {
            const float w = scores.at(static_cast<std::size_t>(kv));
            for (int d = 0; d < dHead; ++d) {
                output.at(0, colOffset + d) += w * request.layerCache->selfValue.at(kv, colOffset + d);
            }
        }
    }

    // Output projection.
    CpuTensor projected = matmulTransposed(output, request.layerWeights.selfAttnOutWeight);
    addBiasInPlace(projected, request.layerWeights.selfAttnOutBias);
    return projected;
}

/**
 * @brief Cross-attention using pre-computed encoder KV cache.
 */
CpuTensor decoderCrossAttention(const DecoderCrossAttentionRequest &request)
{
    const int dModel = request.x.cols;
    const int dHead = dModel / request.nHead;
    const int encoderLen = request.layerCache.crossKey.rows;

    // Project Q from the decoder hidden state.
    CpuTensor q = matmulTransposed(request.x, request.layerWeights.crossQueryWeight);
    addBiasInPlace(q, request.layerWeights.crossQueryBias);

    const float scale = 1.0F / std::sqrt(static_cast<float>(dHead));
    CpuTensor output(1, dModel);

    for (int h = 0; h < request.nHead; ++h) {
        const int colOffset = h * dHead;

        // Compute attention scores against cached encoder keys.
        std::vector<float> scores(static_cast<std::size_t>(encoderLen));
        for (int kv = 0; kv < encoderLen; ++kv) {
            float dot = 0.0F;
            for (int d = 0; d < dHead; ++d) {
                dot += q.at(0, colOffset + d) * request.layerCache.crossKey.at(kv, colOffset + d);
            }
            scores.at(static_cast<std::size_t>(kv)) = dot * scale;
        }

        // Softmax.
        float maxScore = -std::numeric_limits<float>::infinity();
        for (const float s : scores) {
            maxScore = std::max(maxScore, s);
        }
        float sumExp = 0.0F;
        for (float &s : scores) {
            s = std::exp(s - maxScore);
            sumExp += s;
        }
        if (sumExp > 0.0F) {
            const float invSum = 1.0F / sumExp;
            for (float &s : scores) {
                s *= invSum;
            }
        }

        // Weighted sum of cached encoder values.
        for (int d = 0; d < dHead; ++d) {
            output.at(0, colOffset + d) = 0.0F;
        }
        for (int kv = 0; kv < encoderLen; ++kv) {
            const float w = scores.at(static_cast<std::size_t>(kv));
            for (int d = 0; d < dHead; ++d) {
                output.at(0, colOffset + d) += w * request.layerCache.crossValue.at(kv, colOffset + d);
            }
        }
    }

    // Output projection.
    CpuTensor projected = matmulTransposed(output, request.layerWeights.crossAttnOutWeight);
    addBiasInPlace(projected, request.layerWeights.crossAttnOutBias);
    return projected;
}

/**
 * @brief Feed-forward network for one decoder layer.
 */
CpuTensor decoderFeedForward(const CpuTensor &x, const CpuDecoderLayerWeights &layer)
{
    CpuTensor hidden = matmulTransposed(x, layer.ffn1Weight);
    addBiasInPlace(hidden, layer.ffn1Bias);
    geluInPlace(hidden);
    CpuTensor output = matmulTransposed(hidden, layer.ffn2Weight);
    addBiasInPlace(output, layer.ffn2Bias);
    return output;
}

} // namespace

CpuTensor decoderStep(int tokenId,
                       const CpuDecoderWeights &weights,
                       const CpuModelConfig &config,
                       CpuKVCache *cache)
{
    const int position = cache->currentPosition;
    const int dModel = config.textStateSize;

    // Token embedding + positional embedding for the current position.
    CpuTensor x(1, dModel);
    const std::span<const float> tokenRow = weights.tokenEmbedding.row(tokenId);
    const std::span<const float> posRow = weights.positionalEmbedding.row(position);
    for (int c = 0; c < dModel; ++c) {
        const auto columnIndex = static_cast<std::size_t>(c);
        x.at(0, c) = tokenRow.subspan(columnIndex, 1).front() + posRow.subspan(columnIndex, 1).front();
    }

    // Decoder transformer layers.
    for (int i = 0; i < config.textLayerCount; ++i) {
        const CpuDecoderLayerWeights &layerWeights = weights.layers.at(static_cast<std::size_t>(i));
        CpuKVCacheLayer &layerCache = cache->layers.at(static_cast<std::size_t>(i));

        // Masked self-attention with pre-norm and residual.
        CpuTensor normed = x;
        layerNormInPlace(normed, layerWeights.selfAttnLnGamma, layerWeights.selfAttnLnBeta);
        const CpuTensor selfAttnOut = decoderSelfAttention(DecoderSelfAttentionRequest{
            .x = normed,
            .layerWeights = layerWeights,
            .layerCache = &layerCache,
            .position = position,
            .nHead = config.textHeadCount,
        });
        addInPlace(x, selfAttnOut);

        // Cross-attention with pre-norm and residual.
        CpuTensor crossNormed = x;
        layerNormInPlace(crossNormed, layerWeights.crossAttnLnGamma, layerWeights.crossAttnLnBeta);
        const CpuTensor crossAttnOut = decoderCrossAttention(DecoderCrossAttentionRequest{
            .x = crossNormed,
            .layerWeights = layerWeights,
            .layerCache = layerCache,
            .nHead = config.textHeadCount,
        });
        addInPlace(x, crossAttnOut);

        // FFN with pre-norm and residual.
        CpuTensor ffnNormed = x;
        layerNormInPlace(ffnNormed, layerWeights.ffnLnGamma, layerWeights.ffnLnBeta);
        const CpuTensor ffnOut = decoderFeedForward(ffnNormed, layerWeights);
        addInPlace(x, ffnOut);
    }

    // Final layer norm.
    layerNormInPlace(x, weights.lnGamma, weights.lnBeta);

    // Project to vocabulary logits: x @ tokenEmbedding^T (weight tying).
    CpuTensor logits = matmulTransposed(x, weights.tokenEmbedding);

    cache->currentPosition = position + 1;
    return logits;
}
