#include "transcription/transcriptionengine.h"

#include "transcription/whispercpptranscriber.h"

std::shared_ptr<const TranscriptionEngine> createTranscriptionEngine(const TranscriberConfig &config)
{
    return createWhisperCppTranscriptionEngine(config);
}
