#pragma once

#include <QMetaType>
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
 * @brief Runtime inspection data kept separate from static backend capabilities.
 */
struct RuntimeDiagnostics {
    /// Stable backend identifier used in diagnostics.
    QString backendName;
    /// Human-readable runtime and device summary.
    QString runtimeDescription;
    /// Loaded-model description when a model is available.
    QString loadedModelDescription;
};

/**
 * @brief Normalized runtime audio payload.
 */
struct NormalizedAudio {
    /// Mono float32 samples ready for runtime ingestion.
    std::vector<float> samples;
    /// Sample rate of the normalized audio. Kept at 16 kHz.
    int sampleRate = 16000;
    /// Channel count of the normalized audio. Kept at one channel.
    int channels = 1;

    /**
     * @brief Reports whether the normalized payload contains any samples.
     * @return `true` when at least one audio sample is present.
     */
    [[nodiscard]] bool isValid() const { return !samples.empty(); }
};

/**
 * @brief One normalized streaming audio unit passed into a transcription session.
 */
struct AudioChunk {
    /// Mono float32 samples for this chunk.
    std::vector<float> samples;
    /// Sample rate of the chunk payload.
    int sampleRate = 16000;
    /// Channel count of the chunk payload.
    int channels = 1;
    /// Start frame offset of this chunk within the utterance stream.
    std::int64_t streamOffsetFrames = 0;

    /**
     * @brief Reports whether the chunk contains usable audio samples.
     * @return `true` when at least one sample is present.
     */
    [[nodiscard]] bool isValid() const { return !samples.empty(); }
};

/**
 * @brief Stable transcript event categories emitted by streaming sessions.
 */
enum class TranscriptEventKind : std::uint8_t {
    Partial,
    Final,
};

/**
 * @brief One transcript event produced by a backend session.
 */
struct TranscriptEvent {
    /// Whether this event is partial or final.
    TranscriptEventKind kind = TranscriptEventKind::Partial;
    /// Transcript text payload for this event.
    QString text;
    /// Optional inclusive event start timestamp in milliseconds.
    std::int64_t startMs = -1;
    /// Optional exclusive event end timestamp in milliseconds.
    std::int64_t endMs = -1;
};

/**
 * @brief Result of one streaming session operation.
 */
struct TranscriptUpdate {
    /// Zero or more transcript events emitted by the operation.
    std::vector<TranscriptEvent> events;
    /// Structured runtime failure when the operation did not succeed.
    RuntimeError error;

    /**
     * @brief Reports whether this update completed without a runtime error.
     * @return `true` when `error` is clear.
     */
    [[nodiscard]] bool isOk() const { return error.isOk(); }
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

Q_DECLARE_METATYPE(RuntimeErrorCode)
Q_DECLARE_METATYPE(RuntimeError)
Q_DECLARE_METATYPE(BackendCapabilities)
