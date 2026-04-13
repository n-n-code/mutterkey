#pragma once

#include "asr/nativecpu/cputensor.h"

#include <span>
#include <vector>

/**
 * @file
 * @brief Log-mel spectrogram frontend for the native CPU runtime.
 *
 * Implements the real audio feature extraction pipeline used by Whisper-family
 * models: STFT with Hann window, mel filterbank application, and log transform.
 */

/**
 * @brief Configuration for the mel spectrogram extraction.
 */
struct CpuMelConfig {
    /// Audio sample rate in Hz.
    int sampleRate = 16000;
    /// FFT window size in samples (25 ms at 16 kHz).
    int fftSize = 400;
    /// Hop length in samples (10 ms at 16 kHz).
    int hopLength = 160;
    /// Number of mel filter bands.
    int melBands = 80;
    /// Maximum number of output frames (30 s padded).
    int maxFrames = 3000;
};

/**
 * @brief Pre-computed mel filterbank loaded from the model file.
 *
 * The filterbank is stored as a flat (melBands x fftBins) matrix where
 * fftBins = fftSize / 2 + 1.
 */
struct CpuMelFilterBank {
    /// Filterbank coefficients: melBands rows of fftBins columns.
    std::vector<float> filters;
    /// Number of mel bands (rows).
    int melBands = 0;
    /// Number of FFT frequency bins (cols) = fftSize / 2 + 1.
    int fftBins = 0;

    /**
     * @brief Returns whether the filterbank metadata and coefficient storage are populated.
     * @return `true` when the filterbank contains at least one band and FFT bin.
     */
    [[nodiscard]] bool isValid() const { return !filters.empty() && melBands > 0 && fftBins > 0; }
};

/**
 * @brief Parameters for synthesizing a mel filterbank when the model does not ship one.
 *
 * This spec is used by `computeMelFilterBank()` to rebuild a filterbank that
 * matches the active FFT layout and sample rate.
 */
struct CpuMelFilterBankSpec {
    /// Audio sample rate in Hz.
    int sampleRate = 16000;
    /// FFT window size used to derive the number of frequency bins.
    int fftSize = 400;
    /// Number of mel bands to generate.
    int melBands = 80;
};

/**
 * @brief Extracts a log-mel spectrogram from raw PCM audio.
 *
 * Applies a Hann-windowed STFT, mel filterbank, and log transform. The output
 * is padded or trimmed to exactly config.maxFrames columns.
 *
 * @param samples Mono float32 PCM samples at the configured sample rate.
 * @param filterBank Pre-computed mel filterbank (typically loaded from model).
 * @param config Spectrogram extraction parameters.
 * @return Tensor of shape (melBands, maxFrames) suitable for encoder input.
 */
[[nodiscard]] CpuTensor extractLogMelSpectrogram(std::span<const float> samples,
                                                  const CpuMelFilterBank &filterBank,
                                                  const CpuMelConfig &config);

/**
 * @brief Computes a mel filterbank from frequency parameters.
 *
 * Used as a fallback when the model file does not include a pre-computed
 * filterbank. Uses the HTK mel scale.
 *
 * @param spec Filterbank synthesis parameters.
 * @return Computed filterbank.
 */
[[nodiscard]] CpuMelFilterBank computeMelFilterBank(const CpuMelFilterBankSpec &spec);
