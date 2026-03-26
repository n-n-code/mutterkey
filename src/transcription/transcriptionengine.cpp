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

    [[nodiscard]] std::shared_ptr<const TranscriptionModelHandle> loadModel(RuntimeError *error) const override
    {
        return WhisperCppTranscriber::loadModelHandle(m_config, error);
    }

    [[nodiscard]] std::unique_ptr<TranscriptionSession>
    createSession(std::shared_ptr<const TranscriptionModelHandle> model) const override
    {
        return WhisperCppTranscriber::createSession(m_config, std::move(model));
    }

private:
    TranscriberConfig m_config;
};

} // namespace

std::shared_ptr<const TranscriptionEngine> createTranscriptionEngine(const TranscriberConfig &config)
{
    return std::make_shared<WhisperCppTranscriptionEngine>(config);
}
