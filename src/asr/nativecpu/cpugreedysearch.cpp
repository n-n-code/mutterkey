#include "asr/nativecpu/cpugreedysearch.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

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

bool isSuppressedToken(int tokenId, const CpuGreedySearchConfig &config)
{
    return std::ranges::find(config.suppressedTokenIds, tokenId) != config.suppressedTokenIds.end();
}

int argmaxAllowed(std::span<const float> values, const CpuGreedySearchConfig &config)
{
    int bestIndex = -1;
    float bestValue = -std::numeric_limits<float>::infinity();
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto tokenId = static_cast<int>(index);
        if (isSuppressedToken(tokenId, config)) {
            continue;
        }
        const float value = values.subspan(index, 1).front();
        if (value > bestValue) {
            bestValue = value;
            bestIndex = tokenId;
        }
    }
    return bestIndex >= 0 ? bestIndex : argmax(values);
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

    // Feed initial prompt tokens: SOT first, then packaged prompt tokens.
    // Fall back to the Whisper-en transcribe/no_timestamps defaults when the
    // package manifest does not provide an explicit prompt sequence.
    std::vector<int> promptSequence;
    promptSequence.reserve(1 + std::max<std::size_t>(3, searchConfig.initialPromptTokenIds.size()));
    promptSequence.push_back(searchConfig.sotTokenId);
    if (!searchConfig.initialPromptTokenIds.empty()) {
        promptSequence.insert(promptSequence.end(),
                              searchConfig.initialPromptTokenIds.begin(),
                              searchConfig.initialPromptTokenIds.end());
    } else {
        promptSequence.push_back(searchConfig.languageTokenId);
        promptSequence.push_back(searchConfig.transcribeTokenId);
        promptSequence.push_back(searchConfig.noTimestampsTokenId);
    }

    CpuTensor logits;
    for (const int promptToken : promptSequence) {
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

        const int nextToken = argmaxAllowed(logits.row(0), searchConfig);
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
