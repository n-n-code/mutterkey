#include "transcription/cpudecodergraph.h"

#include <cmath>
#include <limits>

CpuTemplateMatchResult
decodeCpuTemplatePhrase(const std::vector<float> &features,
                        const std::vector<CpuDecodedPhraseTemplate> &phrases,
                        float maxDistance)
{
    CpuTemplateMatchResult bestMatch{
        .matched = false,
        .text = {},
        .distance = std::numeric_limits<float>::infinity(),
    };

    if (features.empty() || phrases.empty() || maxDistance <= 0.0F) {
        return bestMatch;
    }

    for (const CpuDecodedPhraseTemplate &phrase : phrases) {
        if (phrase.featureProfile.size() != features.size()) {
            continue;
        }

        float squaredDistance = 0.0F;
        auto featureIt = features.cbegin();
        auto phraseIt = phrase.featureProfile.cbegin();
        for (; featureIt != features.cend() && phraseIt != phrase.featureProfile.cend(); ++featureIt, ++phraseIt) {
            const float delta = *featureIt - *phraseIt;
            squaredDistance += delta * delta;
        }

        const float distance = std::sqrt(squaredDistance);
        if (distance < bestMatch.distance) {
            bestMatch.distance = distance;
            bestMatch.text = phrase.text;
        }
    }

    bestMatch.matched = !bestMatch.text.isEmpty() && bestMatch.distance <= maxDistance;
    return bestMatch;
}
