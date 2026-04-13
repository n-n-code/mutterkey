#include "asr/nativecpu/cpufeatureextractor.h"

#include <algorithm>
#include <cmath>

std::vector<float> extractCpuEnergyProfile(std::span<const float> samples, int binCount)
{
    if (samples.empty() || binCount <= 0) {
        return {};
    }

    std::vector<float> profile(static_cast<std::size_t>(binCount), 0.0F);
    const std::size_t sampleCount = samples.size();
    auto profileIt = profile.begin();
    for (int binIndex = 0; binIndex < binCount; ++binIndex) {
        const std::size_t start = (sampleCount * static_cast<std::size_t>(binIndex)) / static_cast<std::size_t>(binCount);
        const std::size_t end =
            (sampleCount * static_cast<std::size_t>(binIndex + 1)) / static_cast<std::size_t>(binCount);
        if (end <= start) {
            ++profileIt;
            continue;
        }

        float sum = 0.0F;
        for (const float sample : samples.subspan(start, end - start)) {
            sum += std::abs(sample);
        }
        *profileIt = sum / static_cast<float>(end - start);
        ++profileIt;
    }

    float squaredNorm = 0.0F;
    for (const float value : profile) {
        squaredNorm += value * value;
    }

    if (squaredNorm <= 0.0F) {
        return {};
    }

    const float norm = std::sqrt(squaredNorm);
    for (float &value : profile) {
        value /= norm;
    }
    return profile;
}

float peakAbsoluteSample(std::span<const float> samples)
{
    float peak = 0.0F;
    for (const float sample : samples) {
        peak = std::max(peak, std::abs(sample));
    }
    return peak;
}
