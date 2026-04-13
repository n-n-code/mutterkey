#include "asr/streaming/transcriptioncompat.h"

#include "audio/recordingnormalizer.h"
#include "asr/streaming/audiochunker.h"
#include "asr/streaming/transcriptassembler.h"

#include <utility>

namespace {

RuntimeError makeRuntimeError(RuntimeErrorCode code, QString message)
{
    return RuntimeError{.code = code, .message = std::move(message)};
}

TranscriptionResult makeFailure(RuntimeError error)
{
    return TranscriptionResult{.success = false, .text = {}, .error = std::move(error)};
}

} // namespace

TranscriptionResult transcribeRecordingViaStreaming(TranscriptionSession &session,
                                                    const Recording &recording,
                                                    const RecordingNormalizer &normalizer)
{
    QString errorMessage;
    NormalizedAudio normalizedAudio;
    if (!normalizer.normalizeForRuntime(recording, &normalizedAudio, &errorMessage)) {
        return makeFailure(makeRuntimeError(RuntimeErrorCode::AudioNormalizationFailed, errorMessage));
    }

    const AudioChunker chunker;
    std::vector<AudioChunk> chunks;
    if (!chunker.chunkAudio(normalizedAudio, &chunks, &errorMessage)) {
        return makeFailure(makeRuntimeError(RuntimeErrorCode::AudioNormalizationFailed, errorMessage));
    }

    TranscriptAssembler assembler;
    for (const AudioChunk &chunk : chunks) {
        const TranscriptUpdate update = session.pushAudioChunk(chunk);
        if (!update.isOk()) {
            return makeFailure(update.error);
        }
        assembler.applyUpdate(update);
    }

    const TranscriptUpdate finalUpdate = session.finish();
    if (!finalUpdate.isOk()) {
        return makeFailure(finalUpdate.error);
    }
    assembler.applyUpdate(finalUpdate);

    return TranscriptionResult{
        .success = true,
        .text = assembler.finalTranscript(),
        .error = {},
    };
}
