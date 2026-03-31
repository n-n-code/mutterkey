#pragma once

#include "config.h"
#include "transcription/modelpackage.h"
#include "transcription/transcriptionengine.h"
#include "transcription/transcriptiontypes.h"

#include <atomic>
#include <memory>

struct whisper_context;
struct whisper_state;
class WhisperCppModelHandle;

/**
 * @file
 * @brief Embedded whisper.cpp backend integration.
 */

/**
 * @brief In-process transcription backend backed by vendored whisper.cpp.
 *
 * The backend lazily initializes its `whisper_context` and keeps ownership in a
 * RAII-managed smart pointer so service shutdown and worker teardown stay
 * deterministic.
 */
class WhisperCppTranscriber final : public TranscriptionSession
{
public:
    /**
     * @brief Loads an immutable whisper.cpp model handle.
     * @param config Whisper settings copied into the handle.
     * @param error Optional output for model validation or load failures.
     * @return Shared immutable model handle on success.
     */
    [[nodiscard]] static std::shared_ptr<const TranscriptionModelHandle>
    loadModelHandle(const TranscriberConfig &config, RuntimeError *error = nullptr);

    /**
     * @brief Creates a mutable session from a generic app-owned model handle.
     * @param config Whisper settings copied into the session.
     * @param model Shared immutable model handle.
     * @return Session on success, otherwise `nullptr` when the model is not a whisper handle.
     */
    [[nodiscard]] static std::unique_ptr<TranscriptionSession>
    createSession(const TranscriberConfig &config, std::shared_ptr<const TranscriptionModelHandle> model);

    /**
     * @brief Creates a mutable session from a loaded model handle.
     * @param config Whisper settings copied into the session.
     * @param model Shared immutable model handle.
     */
    WhisperCppTranscriber(TranscriberConfig config, std::shared_ptr<const TranscriptionModelHandle> model);

    /**
     * @brief Releases the owned whisper.cpp state.
     */
    ~WhisperCppTranscriber() override;

    WhisperCppTranscriber(const WhisperCppTranscriber &) = delete;
    WhisperCppTranscriber &operator=(const WhisperCppTranscriber &) = delete;
    WhisperCppTranscriber(WhisperCppTranscriber &&) = delete;
    WhisperCppTranscriber &operator=(WhisperCppTranscriber &&) = delete;

    /**
     * @brief Returns the backend name used in diagnostics.
     * @return Human-readable backend identifier.
     */
    [[nodiscard]] static QString backendNameStatic();

    /**
     * @brief Returns the static capability snapshot for the whisper.cpp runtime.
     * @return Capability data derived from the embedded backend integration.
     */
    [[nodiscard]] static BackendCapabilities capabilitiesStatic();
    /**
     * @brief Returns runtime diagnostics for the embedded whisper runtime.
     * @return Runtime/device diagnostic data for this backend.
     */
    [[nodiscard]] static RuntimeDiagnostics diagnosticsStatic();
    [[nodiscard]] QString backendName() const override;

    /**
     * @brief Eagerly initializes the whisper.cpp context.
     * @param error Optional output for initialization failures.
     * @return `true` when the backend is ready for transcription.
     */
    bool warmup(RuntimeError *error = nullptr) override;

    /**
     * @brief Appends one normalized chunk to the current utterance buffer.
     * @param chunk Product-owned normalized audio chunk.
     * @return Empty update on success or a structured runtime failure.
     */
    [[nodiscard]] TranscriptUpdate pushAudioChunk(const AudioChunk &chunk) override;

    /**
     * @brief Decodes the buffered utterance and emits final transcript events.
     * @return Structured transcript update for the completed utterance.
     */
    [[nodiscard]] TranscriptUpdate finish() override;

    /**
     * @brief Requests cooperative cancellation of active backend work.
     */
    [[nodiscard]] TranscriptUpdate cancel() override;

private:
    /**
     * @brief Custom deleter for owned whisper.cpp state.
     * @param state State pointer to release.
     */
    static void freeState(whisper_state *state) noexcept;

    /**
     * @brief Creates the mutable decode state on first use.
     * @param error Optional output for initialization failures.
     * @return `true` when the backend state is available.
     */
    bool ensureState(RuntimeError *error = nullptr);

    /// Immutable whisper.cpp runtime configuration.
    TranscriberConfig m_config;
    /// Shared immutable whisper model handle loaded by the engine.
    std::shared_ptr<const WhisperCppModelHandle> m_model;
    /// Owned whisper.cpp decode state with RAII cleanup.
    std::unique_ptr<whisper_state, void (*)(whisper_state *)> m_state;
    /// Session-owned normalized audio accumulated through the streaming interface.
    std::vector<float> m_bufferedSamples;
    /// Cooperative cancellation flag checked by whisper.cpp abort callback.
    std::atomic_bool m_cancelRequested = false;
};

/**
 * @brief Creates the embedded whisper.cpp engine implementation.
 * @param config Whisper runtime configuration copied into the engine.
 * @return Engine backed by the vendored whisper.cpp integration.
 */
[[nodiscard]] std::shared_ptr<const TranscriptionEngine> createWhisperCppTranscriptionEngine(const TranscriberConfig &config);
