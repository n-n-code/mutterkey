#pragma once

#include "audio/recording.h"
#include "config.h"
#include "transcription/whispercpptranscriber.h"

#include <QObject>

class TranscriptionWorker final : public QObject
{
    Q_OBJECT

public:
    explicit TranscriptionWorker(TranscriberConfig config, QObject *parent = nullptr);
    ~TranscriptionWorker() override = default;

    Q_DISABLE_COPY_MOVE(TranscriptionWorker)

    [[nodiscard]] QString backendName() const;
    bool warmup(QString *errorMessage = nullptr);
    void transcribe(const Recording &recording);

signals:
    void transcriptionReady(const QString &text);
    void transcriptionFailed(const QString &errorMessage);

private:
    WhisperCppTranscriber m_transcriber;
};
