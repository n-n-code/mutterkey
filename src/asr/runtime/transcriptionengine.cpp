#include "asr/runtime/transcriptionengine.h"

#include "asr/nativecpu/cpureferencetranscriber.h"
#include "asr/runtime/runtimeselector.h"

#if defined(MUTTERKEY_WITH_LEGACY_WHISPER)
#include "asr/legacy/whispercpptranscriber.h"
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
