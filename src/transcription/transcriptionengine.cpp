#include "transcription/transcriptionengine.h"

#include "transcription/whispercpptranscriber.h"

#include <utility>

namespace {

class WhisperCppTranscriptionEngine final : public TranscriptionEngine
{
public:
    explicit WhisperCppTranscriptionEngine(TranscriberConfig config)
        : m_config(std::move(config))
    {
    }

    [[nodiscard]] BackendCapabilities capabilities() const override
    {
        return WhisperCppTranscriber::capabilitiesStatic();
    }

    [[nodiscard]] std::unique_ptr<TranscriptionSession> createSession() const override
    {
        return std::make_unique<WhisperCppTranscriber>(m_config);
    }

private:
    TranscriberConfig m_config;
};

} // namespace

std::shared_ptr<const TranscriptionEngine> createTranscriptionEngine(const TranscriberConfig &config)
{
    return std::make_shared<WhisperCppTranscriptionEngine>(config);
}
