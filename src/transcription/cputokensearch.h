#pragma once

#include "transcription/cpudecodergraph.h"
#include "transcription/cputokenizer.h"

#include <QString>

#include <vector>

/**
 * @file
 * @brief Search-policy helpers for the native CPU runtime.
 */

/**
 * @brief Input request for one native CPU token-search pass.
 */
struct CpuTokenSearchRequest {
    /// Extracted normalized feature profile for the utterance.
    std::vector<float> features;
    /// Available phrase templates from the loaded model.
    std::vector<CpuDecodedPhraseTemplate> phraseTemplates;
    /// Maximum accepted distance for a match.
    float maxDistance = 0.0F;
};

/**
 * @brief Result of one native CPU search pass.
 */
struct CpuTokenSearchResult {
    /// `true` when the search policy found a valid transcript.
    bool matched = false;
    /// Final transcript text chosen by the search policy.
    QString transcript;
    /// Decoder tokens derived from the final transcript.
    std::vector<CpuDecodedToken> tokens;
    /// Search distance for the winning candidate.
    float distance = 0.0F;
};

/**
 * @brief Runs the current greedy native CPU search policy.
 * @param request Search request describing features and templates.
 * @return Search result with transcript and intermediate tokens on success.
 */
[[nodiscard]] CpuTokenSearchResult runCpuGreedyTokenSearch(const CpuTokenSearchRequest &request);
