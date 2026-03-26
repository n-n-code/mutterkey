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
     * @param errorMessage Optional destination for a human-readable failure reason.
     * @return `true` if the session is ready for transcription, otherwise `false`.
     */
    virtual bool warmup(QString *errorMessage = nullptr) = 0;

    /**
     * @brief Transcribes a single captured recording.
     * @param recording Captured audio payload to normalize and transcribe.
     * @return Normalized transcription result for the provided recording.
     */
    [[nodiscard]] virtual TranscriptionResult transcribe(const Recording &recording) = 0;

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
     * @brief Returns the backend identifier for sessions created by this engine.
     * @return Short backend name used for logs and diagnostics.
     */
    [[nodiscard]] virtual QString backendName() const = 0;

    /**
     * @brief Creates a new isolated transcription session.
     * @return Newly constructed session that owns its backend state.
     */
    [[nodiscard]] virtual std::unique_ptr<TranscriptionSession> createSession() const = 0;

protected:
    TranscriptionEngine() = default;
};

/**
 * @brief Creates the configured embedded transcription engine.
 * @param config Backend configuration copied into the engine.
 * @return Engine suitable for creating isolated transcription sessions.
 */
[[nodiscard]] std::unique_ptr<TranscriptionEngine> createTranscriptionEngine(const TranscriberConfig &config);
