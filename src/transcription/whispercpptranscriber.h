#pragma once

#include "audio/recordingnormalizer.h"
#include "config.h"
#include "transcription/transcriptionengine.h"
#include "transcription/transcriptiontypes.h"

#include <memory>

struct whisper_context;

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
     * @brief Creates a backend with a fixed transcription configuration.
     * @param config Whisper settings copied into the backend.
     */
    explicit WhisperCppTranscriber(TranscriberConfig config);

    /**
     * @brief Releases the owned whisper.cpp context.
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
    [[nodiscard]] QString backendName() const override;

    /**
     * @brief Eagerly initializes the whisper.cpp context.
     * @param error Optional output for initialization failures.
     * @return `true` when the backend is ready for transcription.
     */
    bool warmup(RuntimeError *error = nullptr) override;

    /**
     * @brief Normalizes and transcribes one captured recording.
     * @param recording Captured audio payload and format metadata.
     * @return Structured transcription result.
     */
    [[nodiscard]] TranscriptionResult transcribe(const Recording &recording) override;

private:
    /**
     * @brief Custom deleter for the owned whisper.cpp context.
     * @param context Context pointer to release.
     */
    static void freeContext(whisper_context *context) noexcept;

    /**
     * @brief Initializes the backend on first use.
     * @param error Optional output for initialization failures.
     * @return `true` when the backend context is available.
     */
    bool ensureInitialized(RuntimeError *error = nullptr);

    /// Immutable whisper.cpp runtime configuration.
    TranscriberConfig m_config;
    /// Audio-format conversion helper used before calling whisper.cpp.
    RecordingNormalizer m_normalizer;
    /// Owned whisper.cpp context with RAII cleanup.
    std::unique_ptr<whisper_context, void (*)(whisper_context *)> m_context;
};
