#include "transcription/transcriptionengine.h"

#include "transcription/cpureferencetranscriber.h"
#include "transcription/runtimeselector.h"

#if defined(MUTTERKEY_WITH_LEGACY_WHISPER)
#include "transcription/whispercpptranscriber.h"
#endif

std::shared_ptr<const TranscriptionEngine> createTranscriptionEngine(const TranscriberConfig &config)
{
    const RuntimeSelection selection = selectRuntimeForConfig(config);
    if (selection.kind == RuntimeSelectionKind::CpuReference) {
        return createCpuReferenceTranscriptionEngine(config);
    }

#if defined(MUTTERKEY_WITH_LEGACY_WHISPER)
    return createWhisperCppTranscriptionEngine(config);
#else
    return createCpuReferenceTranscriptionEngine(config);
#endif
}
