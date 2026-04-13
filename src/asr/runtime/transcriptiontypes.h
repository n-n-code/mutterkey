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
    InvalidModelPackage,
    UnsupportedModelPackageVersion,
    ModelIntegrityFailed,
    IncompatibleModelPackage,
    ModelTooLarge,
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
    /// Human-readable explanation for why this runtime was selected.
    QString selectionReason;
    /// Human-readable runtime and device summary.
    QString runtimeDescription;
    /// Loaded-model description when a model is available.
    QString loadedModelDescription;
};

/**
 * @brief Product-owned immutable metadata about a validated model artifact.
 */
struct ModelMetadata {
    /// Stable product-owned package identifier.
    QString packageId;
    /// Human-readable package/model name.
    QString displayName;
    /// Optional package version string.
    QString packageVersion;
    /// Runtime family this artifact belongs to.
    QString runtimeFamily;
    /// Source format imported or packaged by Mutterkey.
    QString sourceFormat;
    /// Backend-facing model format marker such as `ggml`.
    QString modelFormat;
    /// Model family or architecture string when known.
    QString architecture;
    /// Language profile such as `en` or `multilingual`.
    QString languageProfile;
    /// Quantization metadata when known.
    QString quantization;
    /// Tokenizer metadata when known.
    QString tokenizer;
    /// Raw-path compatibility marker for migration diagnostics.
    bool legacyCompatibility = false;
    /// Vocabulary size when known.
    int vocabularySize = 0;
    /// Audio context size when known.
    int audioContext = 0;
    /// Audio state size when known.
    int audioState = 0;
    /// Audio attention head count when known.
    int audioHeadCount = 0;
    /// Audio layer count when known.
    int audioLayerCount = 0;
    /// Text context size when known.
    int textContext = 0;
    /// Text state size when known.
    int textState = 0;
    /// Text attention head count when known.
    int textHeadCount = 0;
    /// Text layer count when known.
    int textLayerCount = 0;
    /// Mel filter count when known.
    int melCount = 0;
    /// Backend-specific format type value when known.
    int formatType = 0;
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
Q_DECLARE_METATYPE(ModelMetadata)
