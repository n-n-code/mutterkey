#pragma once

#include <QString>

#include <vector>

/**
 * @file
 * @brief Shared value types exchanged by the transcription pipeline.
 */

/**
 * @brief Result of a single transcription attempt.
 */
struct TranscriptionResult {
    /// `true` when transcription completed successfully.
    bool success = false;
    /// Final recognized text when `success` is `true`.
    QString text;
    /// Human-readable failure reason when `success` is `false`.
    QString error;
};

/**
 * @brief Normalized Whisper input audio.
 */
struct NormalizedAudio {
    /// Mono float32 samples ready for whisper.cpp consumption.
    std::vector<float> samples;
    /// Sample rate of the normalized audio. Kept at Whisper's required 16 kHz.
    int sampleRate = 16000;
    /// Channel count of the normalized audio. Kept at one channel.
    int channels = 1;

    /**
     * @brief Reports whether the normalized payload contains any samples.
     * @return `true` when at least one audio sample is present.
     */
    [[nodiscard]] bool isValid() const { return !samples.empty(); }
};
