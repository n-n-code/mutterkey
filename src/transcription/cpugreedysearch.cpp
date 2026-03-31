#include "transcription/cpugreedysearch.h"

#include <array>
#include <cmath>
#include <limits>

namespace {

/**
 * @brief Computes softmax probability for a single class from raw logits.
 */
float logitToProb(std::span<const float> logits, int classIndex)
{
    float maxLogit = -std::numeric_limits<float>::infinity();
    for (const float v : logits) {
        maxLogit = std::max(maxLogit, v);
    }
    float sumExp = 0.0F;
    for (const float v : logits) {
        sumExp += std::exp(v - maxLogit);
    }
    return std::exp(logits.subspan(static_cast<std::size_t>(classIndex), 1).front() - maxLogit) / sumExp;
}

bool isTimestampToken(int tokenId, const CpuGreedySearchConfig &config)
{
    return tokenId >= config.timestampBeginId && tokenId <= config.timestampEndId;
}

CpuDecodedTokenKind classifyToken(int tokenId, const CpuGreedySearchConfig &config)
{
    if (isTimestampToken(tokenId, config)) {
        return CpuDecodedTokenKind::Timestamp;
    }
    if (tokenId == config.sotTokenId || tokenId == config.eotTokenId
        || tokenId == config.noSpeechTokenId || tokenId == config.noTimestampsTokenId
        || tokenId == config.languageTokenId || tokenId == config.transcribeTokenId) {
        return CpuDecodedTokenKind::Control;
    }
    return CpuDecodedTokenKind::Lexical;
}

} // namespace

CpuGreedySearchResult runCpuGreedySearch(const CpuTensor &encoderOutput,
                                          const CpuWhisperModelWeights &weights,
                                          CpuKVCache *cache,
                                          const CpuGreedySearchConfig &searchConfig)
{
    CpuGreedySearchResult result;
    result.isSpeech = true;

    // Initialize cross-attention KV from encoder output.
    if (!cache->crossAttentionReady) {
        initializeCrossAttentionCache(encoderOutput, weights.decoder, weights.config, cache);
    }

    // Feed initial prompt tokens: SOT, language, transcribe, no_timestamps.
    const std::array<int, 4> initialTokens{
        searchConfig.sotTokenId,
        searchConfig.languageTokenId,
        searchConfig.transcribeTokenId,
        searchConfig.noTimestampsTokenId,
    };

    CpuTensor logits;
    for (const int promptToken : initialTokens) {
        logits = decoderStep(promptToken, weights.decoder, weights.config, cache);
    }

    // Check no-speech probability after initial prompt.
    if (!logits.isEmpty()) {
        const float noSpeechProb = logitToProb(logits.row(0), searchConfig.noSpeechTokenId);
        if (noSpeechProb > searchConfig.noSpeechThreshold) {
            result.isSpeech = false;
            return result;
        }
    }

    // Greedy decode loop.
    for (int step = 0; step < searchConfig.maxDecoderTokens; ++step) {
        if (logits.isEmpty()) {
            break;
        }

        const int nextToken = argmax(logits.row(0));
        if (nextToken == searchConfig.eotTokenId) {
            break;
        }

        const CpuDecodedTokenKind kind = classifyToken(nextToken, searchConfig);
        result.tokens.push_back(CpuDecodedToken{
            .id = nextToken,
            .text = {},
            .kind = kind,
        });

        logits = decoderStep(nextToken, weights.decoder, weights.config, cache);
    }

    return result;
}
