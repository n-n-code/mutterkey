#include "transcription/cpumelspectrogram.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

using IndexType = std::size_t;

struct FftBuffers {
    std::vector<float> &real;
    std::vector<float> &imag;
    int length = 0;
};

/**
 * @brief Next power of two >= n.
 */
int nextPowerOfTwo(int n)
{
    if (n <= 1) {
        return 1;
    }

    std::uint32_t p = 1U;
    const auto target = static_cast<std::uint32_t>(n);
    while (p < target) {
        p <<= 1U;
    }
    return static_cast<int>(p);
}

/**
 * @brief In-place radix-2 Cooley-Tukey FFT.
 *
 * Input arrays must have length @p n which must be a power of two.
 * After the call, (real[k], imag[k]) holds the k-th complex DFT coefficient.
 */
void fftRadix2(const FftBuffers &buffers)
{
    auto &realValues = buffers.real;
    auto &imagValues = buffers.imag;
    const auto unsignedLength = static_cast<std::uint32_t>(buffers.length);

    // Bit-reversal permutation.
    for (std::uint32_t i = 1U, j = 0U; i < unsignedLength; ++i) {
        std::uint32_t bit = unsignedLength >> 1U;
        while ((j & bit) != 0U) {
            j ^= bit;
            bit >>= 1U;
        }
        j ^= bit;
        if (i < j) {
            std::swap(realValues.at(i), realValues.at(j));
            std::swap(imagValues.at(i), imagValues.at(j));
        }
    }

    // Butterfly passes.
    for (std::uint32_t halfSize = 1U; halfSize < unsignedLength; halfSize <<= 1U) {
        const auto size = halfSize << 1U;
        const float angle = -std::numbers::pi_v<float> / static_cast<float>(halfSize);
        const float wReal = std::cos(angle);
        const float wImag = std::sin(angle);
        for (std::uint32_t start = 0U; start < unsignedLength; start += size) {
            float curReal = 1.0F;
            float curImag = 0.0F;
            for (std::uint32_t k = 0U; k < halfSize; ++k) {
                const auto even = start + k;
                const auto odd = even + halfSize;
                const float tReal = (curReal * realValues.at(odd)) - (curImag * imagValues.at(odd));
                const float tImag = (curReal * imagValues.at(odd)) + (curImag * realValues.at(odd));
                realValues.at(odd) = realValues.at(even) - tReal;
                imagValues.at(odd) = imagValues.at(even) - tImag;
                realValues.at(even) += tReal;
                imagValues.at(even) += tImag;
                const float nextReal = (curReal * wReal) - (curImag * wImag);
                const float nextImag = (curReal * wImag) + (curImag * wReal);
                curReal = nextReal;
                curImag = nextImag;
            }
        }
    }
}

/**
 * @brief Computes the Hann window of length @p n.
 */
std::vector<float> hannWindow(int n)
{
    std::vector<float> window(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        window.at(static_cast<IndexType>(i)) =
            0.5F * (1.0F - std::cos((2.0F * std::numbers::pi_v<float> * static_cast<float>(i)) / static_cast<float>(n)));
    }
    return window;
}

float hzToMel(float hz)
{
    return 2595.0F * std::log10(1.0F + (hz / 700.0F));
}

float melToHz(float mel)
{
    return 700.0F * (std::pow(10.0F, mel / 2595.0F) - 1.0F);
}

} // namespace

CpuTensor extractLogMelSpectrogram(std::span<const float> samples,
                                    const CpuMelFilterBank &filterBank,
                                    const CpuMelConfig &config)
{
    const int fftN = nextPowerOfTwo(config.fftSize);
    const int fftBins = (fftN / 2) + 1;
    const int numSamples = static_cast<int>(samples.size());
    const int numFrames = std::max(0, (((numSamples - config.fftSize) / config.hopLength) + 1));

    // The radix-2 FFT zero-pads to the next power of two, which changes the
    // number and spacing of frequency bins. If the provided filterbank was built
    // for the unpadded window size (e.g. 201 bins for a 400-point window vs 257
    // bins for the 512-point padded FFT), recompute it so each mel band
    // integrates the correct frequency range.
    CpuMelFilterBank paddedFilterBank;
    const CpuMelFilterBank *filters = &filterBank;
    if (filterBank.fftBins != fftBins) {
        paddedFilterBank = computeMelFilterBank(CpuMelFilterBankSpec{
            .sampleRate = config.sampleRate,
            .fftSize = fftN,
            .melBands = config.melBands,
        });
        filters = &paddedFilterBank;
    }

    const std::vector<float> window = hannWindow(config.fftSize);

    // Allocate output: (melBands, maxFrames), zero-padded if numFrames < maxFrames.
    CpuTensor mel(config.melBands, config.maxFrames);

    // Temporary FFT buffers.
    std::vector<float> fftReal(static_cast<std::size_t>(fftN), 0.0F);
    std::vector<float> fftImag(static_cast<std::size_t>(fftN), 0.0F);
    std::vector<float> powerSpectrum(static_cast<std::size_t>(fftBins), 0.0F);

    const int framesToProcess = std::min(numFrames, config.maxFrames);

    for (int frame = 0; frame < framesToProcess; ++frame) {
        const int offset = frame * config.hopLength;

        // Apply window and zero-pad to FFT length.
        std::ranges::fill(fftReal, 0.0F);
        std::ranges::fill(fftImag, 0.0F);
        for (int i = 0; i < config.fftSize && (offset + i) < numSamples; ++i) {
            const auto sampleIndex = static_cast<IndexType>(offset) + static_cast<IndexType>(i);
            const auto fftIndex = static_cast<IndexType>(i);
            fftReal.at(fftIndex) = samples.subspan(sampleIndex, 1).front() * window.at(fftIndex);
        }

        fftRadix2(FftBuffers{
            .real = fftReal,
            .imag = fftImag,
            .length = fftN,
        });

        // Power spectrum: |X[k]|^2.
        for (int k = 0; k < fftBins; ++k) {
            const auto fftIndex = static_cast<IndexType>(k);
            powerSpectrum.at(fftIndex) =
                (fftReal.at(fftIndex) * fftReal.at(fftIndex)) + (fftImag.at(fftIndex) * fftImag.at(fftIndex));
        }

        // Apply mel filterbank and take log.
        for (int m = 0; m < config.melBands && m < filters->melBands; ++m) {
            const auto filterOffset = static_cast<IndexType>(m) * static_cast<IndexType>(fftBins);
            float energy = 0.0F;
            for (int k = 0; k < fftBins; ++k) {
                const auto fftIndex = static_cast<IndexType>(k);
                energy += filters->filters.at(filterOffset + fftIndex) * powerSpectrum.at(fftIndex);
            }
            // Log with small epsilon to avoid log(0).
            constexpr float kLogEps = 1e-10F;
            mel.at(m, frame) = std::log(std::max(energy, kLogEps));
        }
    }

    // Whisper-style normalization: clamp to max - 8, then shift/scale to [-1, 1].
    float maxVal = -std::numeric_limits<float>::infinity();
    for (const float v : mel.data) {
        maxVal = std::max(maxVal, v);
    }
    const float clampMin = maxVal - 8.0F;
    for (float &v : mel.data) {
        v = std::max(v, clampMin);
        v = (v - clampMin) / 8.0F;    // Scale to [0, 1].
        v = (v * 2.0F) - 1.0F;        // Shift to [-1, 1].
    }

    return mel;
}

CpuMelFilterBank computeMelFilterBank(const CpuMelFilterBankSpec &spec)
{
    const int fftBins = (spec.fftSize / 2) + 1;
    const float nyquist = static_cast<float>(spec.sampleRate) / 2.0F;
    const float melMin = hzToMel(0.0F);
    const float melMax = hzToMel(nyquist);

    // Equally spaced mel points.
    const int numPoints = spec.melBands + 2;
    std::vector<float> melPoints(static_cast<std::size_t>(numPoints));
    for (int i = 0; i < numPoints; ++i) {
        melPoints.at(static_cast<IndexType>(i)) =
            melMin + ((static_cast<float>(i) * (melMax - melMin)) / static_cast<float>(numPoints - 1));
    }

    // Convert mel points back to Hz, then to FFT bin indices.
    std::vector<float> hzPoints(static_cast<std::size_t>(numPoints));
    std::vector<int> binPoints(static_cast<std::size_t>(numPoints));
    for (int i = 0; i < numPoints; ++i) {
        const auto pointIndex = static_cast<IndexType>(i);
        hzPoints.at(pointIndex) = melToHz(melPoints.at(pointIndex));
        binPoints.at(pointIndex) = static_cast<int>(std::floor(
            ((static_cast<float>(spec.fftSize) + 1.0F) * hzPoints.at(pointIndex)) / static_cast<float>(spec.sampleRate)));
    }

    CpuMelFilterBank filterBank;
    filterBank.melBands = spec.melBands;
    filterBank.fftBins = fftBins;
    filterBank.filters.resize(static_cast<std::size_t>(spec.melBands) * fftBins, 0.0F);

    for (int m = 0; m < spec.melBands; ++m) {
        const int left = binPoints.at(static_cast<IndexType>(m));
        const int center = binPoints.at(static_cast<IndexType>(m) + 1U);
        const int right = binPoints.at(static_cast<IndexType>(m) + 2U);
        const auto filterOffset = static_cast<IndexType>(m) * static_cast<IndexType>(fftBins);

        for (int k = left; k < center && k < fftBins; ++k) {
            if (center > left) {
                filterBank.filters.at(filterOffset + static_cast<IndexType>(k)) =
                    static_cast<float>(k - left) / static_cast<float>(center - left);
            }
        }
        for (int k = center; k < right && k < fftBins; ++k) {
            if (right > center) {
                filterBank.filters.at(filterOffset + static_cast<IndexType>(k)) =
                    static_cast<float>(right - k) / static_cast<float>(right - center);
            }
        }
    }

    return filterBank;
}
