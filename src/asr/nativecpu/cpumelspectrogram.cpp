#include "asr/nativecpu/cpumelspectrogram.h"

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

// Whisper uses librosa's default ``htk=False`` mel scale (Slaney's auditory
// toolbox formula): linear below 1 kHz, logarithmic above.
constexpr float kSlaneyMinLogHz = 1000.0F;
constexpr float kSlaneyStepHz = 200.0F / 3.0F; // Linear-region slope.

float kSlaneyLogStep()
{
    return std::log(6.4F) / 27.0F;
}

float hzToMel(float hz)
{
    const float linearMel = hz / kSlaneyStepHz;
    if (hz < kSlaneyMinLogHz) {
        return linearMel;
    }
    const float minLogMel = kSlaneyMinLogHz / kSlaneyStepHz;
    return minLogMel + (std::log(hz / kSlaneyMinLogHz) / kSlaneyLogStep());
}

float melToHz(float mel)
{
    const float minLogMel = kSlaneyMinLogHz / kSlaneyStepHz;
    if (mel < minLogMel) {
        return mel * kSlaneyStepHz;
    }
    return kSlaneyMinLogHz * std::exp((mel - minLogMel) * kSlaneyLogStep());
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

        // Apply mel filterbank and take log10 (matching Whisper's audio.py).
        for (int m = 0; m < config.melBands && m < filters->melBands; ++m) {
            const auto filterOffset = static_cast<IndexType>(m) * static_cast<IndexType>(fftBins);
            float energy = 0.0F;
            for (int k = 0; k < fftBins; ++k) {
                const auto fftIndex = static_cast<IndexType>(k);
                energy += filters->filters.at(filterOffset + fftIndex) * powerSpectrum.at(fftIndex);
            }
            constexpr float kLogEps = 1e-10F;
            mel.at(m, frame) = std::log10(std::max(energy, kLogEps));
        }
    }

    // Whisper normalization from whisper/audio.py:
    //   log_spec = max(log_spec, log_spec.max() - 8.0)
    //   log_spec = (log_spec + 4.0) / 4.0
    // Only the populated frames participate in the max; unused frames are
    // clamped to the floor afterwards so the encoder sees silence-like values
    // for the padded tail.
    float maxVal = -std::numeric_limits<float>::infinity();
    for (int m = 0; m < config.melBands; ++m) {
        for (int t = 0; t < framesToProcess; ++t) {
            maxVal = std::max(maxVal, mel.at(m, t));
        }
    }
    if (!std::isfinite(maxVal)) {
        maxVal = 0.0F;
    }
    const float clampMin = maxVal - 8.0F;
    const float silenceNormalized = (clampMin + 4.0F) / 4.0F;
    for (int m = 0; m < config.melBands; ++m) {
        for (int t = 0; t < config.maxFrames; ++t) {
            if (t >= framesToProcess) {
                mel.at(m, t) = silenceNormalized;
                continue;
            }
            const float clamped = std::max(mel.at(m, t), clampMin);
            mel.at(m, t) = (clamped + 4.0F) / 4.0F;
        }
    }

    return mel;
}

CpuMelFilterBank computeMelFilterBank(const CpuMelFilterBankSpec &spec)
{
    const int fftBins = (spec.fftSize / 2) + 1;
    const float nyquist = static_cast<float>(spec.sampleRate) / 2.0F;
    const float melMin = hzToMel(0.0F);
    const float melMax = hzToMel(nyquist);

    // Equally spaced mel points on the Slaney mel scale.
    const int numPoints = spec.melBands + 2;
    std::vector<float> hzPoints(static_cast<std::size_t>(numPoints));
    for (int i = 0; i < numPoints; ++i) {
        const float mel =
            melMin + ((static_cast<float>(i) * (melMax - melMin)) / static_cast<float>(numPoints - 1));
        hzPoints.at(static_cast<IndexType>(i)) = melToHz(mel);
    }

    // FFT bin frequencies (Hz) for the unpadded bins (0..fftBins-1).
    std::vector<float> fftFreqsHz(static_cast<std::size_t>(fftBins));
    for (int k = 0; k < fftBins; ++k) {
        fftFreqsHz.at(static_cast<IndexType>(k)) =
            (static_cast<float>(k) * static_cast<float>(spec.sampleRate)) / static_cast<float>(spec.fftSize);
    }

    CpuMelFilterBank filterBank;
    filterBank.melBands = spec.melBands;
    filterBank.fftBins = fftBins;
    filterBank.filters.resize(static_cast<std::size_t>(spec.melBands) * fftBins, 0.0F);

    // Construct triangular filters in the Hz domain (librosa parity) and apply
    // Slaney area normalization: 2 / (hz_right - hz_left).
    for (int m = 0; m < spec.melBands; ++m) {
        const float leftHz = hzPoints.at(static_cast<IndexType>(m));
        const float centerHz = hzPoints.at(static_cast<IndexType>(m) + 1U);
        const float rightHz = hzPoints.at(static_cast<IndexType>(m) + 2U);
        const auto filterOffset = static_cast<IndexType>(m) * static_cast<IndexType>(fftBins);
        const float enorm = 2.0F / (rightHz - leftHz);

        for (int k = 0; k < fftBins; ++k) {
            const float f = fftFreqsHz.at(static_cast<IndexType>(k));
            const float lower = (f - leftHz) / (centerHz - leftHz);
            const float upper = (rightHz - f) / (rightHz - centerHz);
            const float value = std::max(0.0F, std::min(lower, upper));
            if (value > 0.0F) {
                filterBank.filters.at(filterOffset + static_cast<IndexType>(k)) = value * enorm;
            }
        }
    }

    return filterBank;
}
