#pragma once

#include "config.h"
#include "transcription/transcriptiontypes.h"

#include <memory>

struct Recording;

/**
 * @file
 * @brief Stable engine/session boundary for embedded transcription backends.
 */

/**
 * @brief Immutable loaded-model interface created by a transcription engine.
 *
 * Handles own validated backend assets and may be shared across multiple
 * independent sessions without exposing backend-specific state to app code.
 */
class TranscriptionModelHandle
{
public:
    virtual ~TranscriptionModelHandle() = default;
    TranscriptionModelHandle(const TranscriptionModelHandle &) = delete;
    TranscriptionModelHandle &operator=(const TranscriptionModelHandle &) = delete;
    TranscriptionModelHandle(TranscriptionModelHandle &&) = delete;
    TranscriptionModelHandle &operator=(TranscriptionModelHandle &&) = delete;

    /**
     * @brief Returns the backend identifier for this loaded model.
     * @return Short backend name used in diagnostics.
     */
    [[nodiscard]] virtual QString backendName() const = 0;

    /**
     * @brief Returns product-owned immutable metadata for the loaded artifact.
     * @return Metadata surfaced without exposing backend-specific handles.
     */
    [[nodiscard]] virtual ModelMetadata metadata() const = 0;

    /**
     * @brief Returns a human-readable description of the loaded model.
     * @return Diagnostic model description such as the resolved model path.
     */
    [[nodiscard]] virtual QString modelDescription() const = 0;

protected:
    TranscriptionModelHandle() = default;
};

/**
 * @brief Mutable per-session transcription interface.
 *
 * Sessions own backend state that may be warmed up, reused, and kept isolated
 * per thread or request flow.
 */
class TranscriptionSession
{
public:
    virtual ~TranscriptionSession() = default;
    TranscriptionSession(const TranscriptionSession &) = delete;
    TranscriptionSession &operator=(const TranscriptionSession &) = delete;
    TranscriptionSession(TranscriptionSession &&) = delete;
    TranscriptionSession &operator=(TranscriptionSession &&) = delete;

    /**
     * @brief Returns the backend identifier for this live session.
     * @return Short backend name used for logs and diagnostics.
     */
    [[nodiscard]] virtual QString backendName() const = 0;

    /**
     * @brief Performs optional backend warmup for this session.
     * @param error Optional destination for a structured failure reason.
     * @return `true` if the session is ready for transcription, otherwise `false`.
     */
    virtual bool warmup(RuntimeError *error = nullptr) = 0;

    /**
     * @brief Ingests one normalized audio chunk into the live session.
     * @param chunk Product-owned normalized audio chunk.
     * @return Any transcript events emitted while ingesting the chunk.
     */
    [[nodiscard]] virtual TranscriptUpdate pushAudioChunk(const AudioChunk &chunk) = 0;

    /**
     * @brief Flushes the current utterance and emits any final transcript events.
     * @return Final transcript events or a structured runtime failure.
     */
    [[nodiscard]] virtual TranscriptUpdate finish() = 0;

    /**
     * @brief Requests cooperative cancellation of any active decode.
     *
     * Implementations should stop in-flight backend work best-effort without
     * using thread interruption.
     */
    [[nodiscard]] virtual TranscriptUpdate cancel() = 0;

protected:
    TranscriptionSession() = default;
};

/**
 * @brief Immutable engine configuration that creates backend sessions.
 *
 * The engine boundary keeps future backend selection and model-loading policy
 * out of the app/service orchestration layers.
 */
class TranscriptionEngine
{
public:
    virtual ~TranscriptionEngine() = default;
    TranscriptionEngine(const TranscriptionEngine &) = delete;
    TranscriptionEngine &operator=(const TranscriptionEngine &) = delete;
    TranscriptionEngine(TranscriptionEngine &&) = delete;
    TranscriptionEngine &operator=(TranscriptionEngine &&) = delete;

    /**
     * @brief Returns the runtime capabilities for sessions created by this engine.
     * @return App-owned capability snapshot suitable for diagnostics.
     */
    [[nodiscard]] virtual BackendCapabilities capabilities() const = 0;

    /**
     * @brief Returns runtime inspection data for this engine instance.
     * @return App-owned runtime diagnostics separate from static capabilities.
     */
    [[nodiscard]] virtual RuntimeDiagnostics diagnostics() const = 0;

    /**
     * @brief Loads an immutable validated model handle for this engine.
     * @param error Optional destination for a structured failure reason.
     * @return Shared loaded-model handle suitable for multiple sessions.
     */
    [[nodiscard]] virtual std::shared_ptr<const TranscriptionModelHandle> loadModel(RuntimeError *error = nullptr) const = 0;

    /**
     * @brief Creates a new isolated transcription session from a loaded model.
     * @param model Shared immutable model handle created by this engine.
     * @return Newly constructed session that owns only mutable backend state.
     */
    [[nodiscard]] virtual std::unique_ptr<TranscriptionSession>
    createSession(std::shared_ptr<const TranscriptionModelHandle> model) const = 0;

protected:
    TranscriptionEngine() = default;
};

/**
 * @brief Creates the configured embedded transcription engine.
 * @param config Backend configuration copied into the engine.
 * @return Engine suitable for creating isolated transcription sessions.
 */
[[nodiscard]] std::shared_ptr<const TranscriptionEngine> createTranscriptionEngine(const TranscriberConfig &config);
