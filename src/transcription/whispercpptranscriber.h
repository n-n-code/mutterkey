#pragma once

#include "audio/recordingnormalizer.h"
#include "config.h"
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
class WhisperCppTranscriber final
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
    ~WhisperCppTranscriber();

    WhisperCppTranscriber(const WhisperCppTranscriber &) = delete;
    WhisperCppTranscriber &operator=(const WhisperCppTranscriber &) = delete;
    WhisperCppTranscriber(WhisperCppTranscriber &&) = delete;
    WhisperCppTranscriber &operator=(WhisperCppTranscriber &&) = delete;

    /**
     * @brief Returns the backend name used in diagnostics.
     * @return Human-readable backend identifier.
     */
    [[nodiscard]] static QString backendName();

    /**
     * @brief Eagerly initializes the whisper.cpp context.
     * @param errorMessage Optional output for initialization failures.
     * @return `true` when the backend is ready for transcription.
     */
    bool warmup(QString *errorMessage = nullptr);

    /**
     * @brief Normalizes and transcribes one captured recording.
     * @param recording Captured audio payload and format metadata.
     * @return Structured transcription result.
     */
    [[nodiscard]] TranscriptionResult transcribe(const Recording &recording);

private:
    /**
     * @brief Custom deleter for the owned whisper.cpp context.
     * @param context Context pointer to release.
     */
    static void freeContext(whisper_context *context) noexcept;

    /**
     * @brief Initializes the backend on first use.
     * @param errorMessage Optional output for initialization failures.
     * @return `true` when the backend context is available.
     */
    bool ensureInitialized(QString *errorMessage = nullptr);

    /// Immutable whisper.cpp runtime configuration.
    TranscriberConfig m_config;
    /// Audio-format conversion helper used before calling whisper.cpp.
    RecordingNormalizer m_normalizer;
    /// Owned whisper.cpp context with RAII cleanup.
    std::unique_ptr<whisper_context, void (*)(whisper_context *)> m_context;
};
