#include "transcription/transcriptionengine.h"

#include "transcription/whispercpptranscriber.h"

#include <memory>
#include <utility>

namespace {

class WhisperCppTranscriptionEngine final : public TranscriptionEngine
{
public:
    explicit WhisperCppTranscriptionEngine(TranscriberConfig config)
        : m_config(std::move(config))
    {
    }

    [[nodiscard]] QString backendName() const override
    {
        return WhisperCppTranscriber::backendNameStatic();
    }

    [[nodiscard]] std::unique_ptr<TranscriptionSession> createSession() const override
    {
        return std::make_unique<WhisperCppTranscriber>(m_config);
    }

private:
    TranscriberConfig m_config;
};

} // namespace

std::unique_ptr<TranscriptionEngine> createTranscriptionEngine(const TranscriberConfig &config)
{
    return std::make_unique<WhisperCppTranscriptionEngine>(config);
}
