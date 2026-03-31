#pragma once

#include <QString>

#include <vector>

/**
 * @file
 * @brief Small native CPU phrase-template decoder helpers.
 */

/**
 * @brief One immutable native CPU phrase template.
 */
struct CpuDecodedPhraseTemplate {
    /// Final transcript text associated with this template.
    QString text;
    /// Normalized energy-profile vector used for nearest-template matching.
    std::vector<float> featureProfile;
};

/**
 * @brief Result of matching one utterance profile against native templates.
 */
struct CpuTemplateMatchResult {
    /// `true` when a template matched within the configured distance threshold.
    bool matched = false;
    /// Final transcript text for the winning template.
    QString text;
    /// Euclidean distance to the winning template.
    float distance = 0.0F;
};

/**
 * @brief Chooses the nearest phrase template for an extracted feature profile.
 * @param features L2-normalized extracted feature profile.
 * @param phrases Available phrase templates from the loaded model.
 * @param maxDistance Maximum accepted Euclidean distance for a successful match.
 * @return Match result with transcript text only when the distance threshold passes.
 */
[[nodiscard]] CpuTemplateMatchResult
decodeCpuTemplatePhrase(const std::vector<float> &features,
                        const std::vector<CpuDecodedPhraseTemplate> &phrases,
                        float maxDistance);
