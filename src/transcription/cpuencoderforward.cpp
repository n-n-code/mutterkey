#include "transcription/cpuencoderforward.h"

#include <algorithm>
#include <cmath>

namespace {

/**
 * @brief Runs multi-head self-attention for the encoder.
 *
 * Q, K, V are projected from the input. K has no bias in Whisper.
 * Per-head attention scores are computed, softmaxed, and applied to V.
 */
CpuTensor encoderSelfAttention(const CpuTensor &x,
                                const CpuEncoderLayerWeights &layer,
                                int nHead)
{
    const int seqLen = x.rows;
    const int dModel = x.cols;
    const int dHead = dModel / nHead;

    // Project Q, K, V: input @ weight^T (+ bias where applicable).
    CpuTensor q = matmulTransposed(x, layer.queryWeight);
    addBiasInPlace(q, layer.queryBias);
    const CpuTensor k = matmulTransposed(x, layer.keyWeight);
    // No bias for key in Whisper.
    CpuTensor v = matmulTransposed(x, layer.valueWeight);
    addBiasInPlace(v, layer.valueBias);

    // Per-head attention.
    const float scale = 1.0F / std::sqrt(static_cast<float>(dHead));
    CpuTensor output(seqLen, dModel);

    for (int h = 0; h < nHead; ++h) {
        const int colOffset = h * dHead;

        // Extract per-head slices.
        const CpuTensor qh = q.columnSlice(colOffset, dHead);
        const CpuTensor kh = k.columnSlice(colOffset, dHead);
        const CpuTensor vh = v.columnSlice(colOffset, dHead);

        // Attention scores: Q_h @ K_h^T / sqrt(d_head).
        CpuTensor scores = matmulTransposed(qh, kh);
        scaleInPlace(scores, scale);
        softmaxRowsInPlace(scores);

        // Weighted values: scores @ V_h.
        const CpuTensor attnOut = matmul(scores, vh);

        // Write back to output columns.
        output.setColumnSlice(colOffset, attnOut);
    }

    // Output projection.
    CpuTensor projected = matmulTransposed(output, layer.attnOutWeight);
    addBiasInPlace(projected, layer.attnOutBias);
    return projected;
}

/**
 * @brief Runs the feed-forward network for one encoder layer.
 */
CpuTensor encoderFeedForward(const CpuTensor &x, const CpuEncoderLayerWeights &layer)
{
    CpuTensor hidden = matmulTransposed(x, layer.ffn1Weight);
    addBiasInPlace(hidden, layer.ffn1Bias);
    geluInPlace(hidden);
    CpuTensor output = matmulTransposed(hidden, layer.ffn2Weight);
    addBiasInPlace(output, layer.ffn2Bias);
    return output;
}

/**
 * @brief Runs one pre-norm encoder transformer layer.
 */
CpuTensor encoderTransformerLayer(const CpuTensor &x,
                                   const CpuEncoderLayerWeights &layer,
                                   int nHead)
{
    // Self-attention block with pre-norm and residual.
    CpuTensor normed = x;
    layerNormInPlace(normed, layer.attnLnGamma, layer.attnLnBeta);
    const CpuTensor attnOut = encoderSelfAttention(normed, layer, nHead);
    CpuTensor residual = x;
    addInPlace(residual, attnOut);

    // FFN block with pre-norm and residual.
    CpuTensor ffnNormed = residual;
    layerNormInPlace(ffnNormed, layer.ffnLnGamma, layer.ffnLnBeta);
    const CpuTensor ffnOut = encoderFeedForward(ffnNormed, layer);
    addInPlace(residual, ffnOut);

    return residual;
}

/**
 * @brief Transposes a (rows, cols) tensor to (cols, rows).
 */
CpuTensor transpose(const CpuTensor &t)
{
    CpuTensor result(t.cols, t.rows);
    for (int r = 0; r < t.rows; ++r) {
        for (int c = 0; c < t.cols; ++c) {
            result.at(c, r) = t.at(r, c);
        }
    }
    return result;
}

} // namespace

CpuTensor runEncoderForward(const CpuTensor &mel,
                             const CpuEncoderWeights &weights,
                             const CpuModelConfig &config)
{
    // mel is (melBands, frames). Conv1d expects (time, channels) = (frames, melBands).
    CpuTensor x = transpose(mel);

    // Conv1 (melBands -> audioState, kernel=3, stride=1, padding=1) + GELU.
    x = conv1d(CpuConv1dRequest{
        .input = x,
        .weight = weights.conv1Weight,
        .bias = weights.conv1Bias,
        .kernelSize = 3,
        .stride = 1,
        .padding = 1,
    });
    geluInPlace(x);

    // Conv2 (audioState -> audioState, kernel=3, stride=2, padding=1) + GELU.
    x = conv1d(CpuConv1dRequest{
        .input = x,
        .weight = weights.conv2Weight,
        .bias = weights.conv2Bias,
        .kernelSize = 3,
        .stride = 2,
        .padding = 1,
    });
    geluInPlace(x);

    // x is now (encoderPositions, audioState). Add positional embedding.
    const int positions = std::min(x.rows, weights.positionalEmbedding.rows);
    for (int r = 0; r < positions; ++r) {
        const std::span<const float> posRow = weights.positionalEmbedding.row(r);
        for (int c = 0; c < x.cols; ++c) {
            const auto columnIndex = static_cast<std::size_t>(c);
            x.at(r, c) += posRow.subspan(columnIndex, 1).front();
        }
    }

    // Transformer layers.
    for (int i = 0; i < config.audioLayerCount; ++i) {
        x = encoderTransformerLayer(x,
                                    weights.layers.at(static_cast<std::size_t>(i)),
                                    config.audioHeadCount);
    }

    // Final layer norm.
    layerNormInPlace(x, weights.lnPostGamma, weights.lnPostBeta);

    return x;
}
