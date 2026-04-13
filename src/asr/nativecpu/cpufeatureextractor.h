#pragma once

#include <span>
#include <vector>

/**
 * @file
 * @brief Small native CPU audio feature extraction helpers.
 */

/**
 * @brief Extracts a normalized absolute-energy profile from mono PCM samples.
 * @param samples Input mono float32 samples.
 * @param binCount Number of output bins to compute.
 * @return L2-normalized energy profile, or an empty vector when inputs are invalid.
 */
[[nodiscard]] std::vector<float> extractCpuEnergyProfile(std::span<const float> samples, int binCount);

/**
 * @brief Computes the peak absolute sample value for a mono PCM buffer.
 * @param samples Input mono float32 samples.
 * @return Peak absolute value, or `0.0f` when empty.
 */
[[nodiscard]] float peakAbsoluteSample(std::span<const float> samples);
