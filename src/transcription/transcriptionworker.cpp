#include "transcription/transcriptionworker.h"

#include <utility>

TranscriptionWorker::TranscriptionWorker(TranscriberConfig config, QObject *parent)
    : QObject(parent)
    , m_transcriber(std::move(config))
{
}

QString TranscriptionWorker::backendName() const
{
    return WhisperCppTranscriber::backendName();
}

bool TranscriptionWorker::warmup(QString *errorMessage)
{
    return m_transcriber.warmup(errorMessage);
}

void TranscriptionWorker::transcribe(const Recording &recording)
{
    const TranscriptionResult result = m_transcriber.transcribe(recording);
    if (!result.success) {
        emit transcriptionFailed(result.error);
        return;
    }

    emit transcriptionReady(result.text);
}
