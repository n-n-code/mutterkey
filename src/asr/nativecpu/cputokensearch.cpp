#include "asr/nativecpu/cputokensearch.h"

CpuTokenSearchResult runCpuGreedyTokenSearch(const CpuTokenSearchRequest &request)
{
    const CpuTemplateMatchResult match =
        decodeCpuTemplatePhrase(request.features, request.phraseTemplates, request.maxDistance);
    if (!match.matched) {
        return CpuTokenSearchResult{
            .matched = false,
            .transcript = {},
            .tokens = {},
            .distance = match.distance,
        };
    }

    const std::vector<CpuDecodedToken> tokens = tokenizeCpuTranscript(match.text);
    return CpuTokenSearchResult{
        .matched = !tokens.empty(),
        .transcript = match.text,
        .tokens = tokens,
        .distance = match.distance,
    };
}
