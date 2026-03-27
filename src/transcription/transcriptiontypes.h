#pragma once

#include <QString>
#include <QStringList>

#include <cstdint>
#include <vector>

/**
 * @file
 * @brief Shared value types exchanged by the transcription pipeline.
 */

/**
 * @brief Stable categories for runtime-layer failures.
 */
enum class RuntimeErrorCode : std::uint8_t {
    None,
    Cancelled,
    InvalidConfig,
    ModelNotFound,
    ModelLoadFailed,
    AudioNormalizationFailed,
    UnsupportedLanguage,
    DecodeFailed,
    InternalRuntimeError,
};

/**
 * @brief Structured runtime-layer failure with user-facing and diagnostic text.
 */
struct RuntimeError {
    /// Stable error category for programmatic handling and tests.
    RuntimeErrorCode code = RuntimeErrorCode::None;
    /// Human-readable summary safe to surface in logs or UI.
    QString message;
    /// Optional extra context for diagnostics.
    QString detail;

    /**
     * @brief Reports whether this value represents success.
     * @return `true` when no runtime error is present.
     */
    [[nodiscard]] bool isOk() const { return code == RuntimeErrorCode::None; }
};

/**
 * @brief Product-owned backend/runtime metadata surfaced to app code.
 */
struct BackendCapabilities {
    /// Stable backend identifier used in diagnostics.
    QString backendName;
    /// Human-readable runtime and device summary for diagnostics.
    QString runtimeDescription;
    /// Supported language codes accepted by this backend.
    QStringList supportedLanguages;
    /// `true` when the backend can auto-detect the spoken language.
    bool supportsAutoLanguage = false;
    /// `true` when the backend supports translation mode.
    bool supportsTranslation = false;
    /// `true` when warmup is a supported preflight operation.
    bool supportsWarmup = false;
};

/**
 * @brief Result of a single transcription attempt.
 */
struct TranscriptionResult {
    /// `true` when transcription completed successfully.
    bool success = false;
    /// Final recognized text when `success` is `true`.
    QString text;
    /// Structured runtime failure when `success` is `false`.
    RuntimeError error;
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

Q_DECLARE_METATYPE(RuntimeErrorCode)
Q_DECLARE_METATYPE(RuntimeError)
Q_DECLARE_METATYPE(BackendCapabilities)
