#pragma once

#include "audio/recordingnormalizer.h"
#include "config.h"
#include "transcription/transcriptiontypes.h"

#include <memory>

struct whisper_context;

class WhisperCppTranscriber final
{
public:
    explicit WhisperCppTranscriber(TranscriberConfig config);
    ~WhisperCppTranscriber();

    WhisperCppTranscriber(const WhisperCppTranscriber &) = delete;
    WhisperCppTranscriber &operator=(const WhisperCppTranscriber &) = delete;
    WhisperCppTranscriber(WhisperCppTranscriber &&) = delete;
    WhisperCppTranscriber &operator=(WhisperCppTranscriber &&) = delete;

    [[nodiscard]] static QString backendName();
    bool warmup(QString *errorMessage = nullptr);
    [[nodiscard]] TranscriptionResult transcribe(const Recording &recording);

private:
    static void freeContext(whisper_context *context) noexcept;
    bool ensureInitialized(QString *errorMessage = nullptr);

    TranscriberConfig m_config;
    RecordingNormalizer m_normalizer;
    std::unique_ptr<whisper_context, void (*)(whisper_context *)> m_context;
};
