#include "transcription/transcriptionworker.h"

#include <cassert>
#include <utility>

TranscriptionWorker::TranscriptionWorker(const TranscriberConfig &config, QObject *parent)
    : TranscriptionWorker(createTranscriptionEngine(config)->createSession(), parent)
{
}

TranscriptionWorker::TranscriptionWorker(std::unique_ptr<TranscriptionSession> transcriber, QObject *parent)
    : QObject(parent)
    , m_transcriber(std::move(transcriber))
{
    assert(m_transcriber != nullptr);
}

QString TranscriptionWorker::backendName() const
{
    return m_transcriber->backendName();
}

bool TranscriptionWorker::warmup(QString *errorMessage)
{
    return m_transcriber->warmup(errorMessage);
}

void TranscriptionWorker::transcribe(const Recording &recording)
{
    const TranscriptionResult result = m_transcriber->transcribe(recording);
    if (!result.success) {
        emit transcriptionFailed(result.error);
        return;
    }

    emit transcriptionReady(result.text);
}
