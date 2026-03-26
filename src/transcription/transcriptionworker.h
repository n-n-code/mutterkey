#pragma once

#include "audio/recording.h"
#include "config.h"
#include "transcription/transcriptionengine.h"

#include <QObject>
#include <memory>

/**
 * @file
 * @brief Background worker object that performs transcription on a dedicated thread.
 */

/**
 * @brief Thread-hosted wrapper around the configured transcription backend.
 *
 * This QObject is intended to live on the service-owned transcription thread.
 * It converts backend results into Qt signals that can be delivered back to the
 * main-thread service object.
 */
class TranscriptionWorker final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates a worker with a fixed backend configuration.
     * @param config Transcriber settings copied into the owned backend.
     * @param parent Optional QObject parent.
     */
    explicit TranscriptionWorker(const TranscriberConfig &config, QObject *parent = nullptr);
    /**
     * @brief Creates a worker around an already-constructed session.
     * @param transcriber Owned session implementation.
     * @param parent Optional QObject parent.
     */
    explicit TranscriptionWorker(std::unique_ptr<TranscriptionSession> transcriber, QObject *parent = nullptr);
    ~TranscriptionWorker() override = default;

    Q_DISABLE_COPY_MOVE(TranscriptionWorker)

    /**
     * @brief Returns the active transcription backend name.
     * @return Human-readable backend identifier.
     */
    [[nodiscard]] QString backendName() const;

    /**
     * @brief Eagerly initializes backend state before the first real transcription.
     * @param errorMessage Optional output for warmup failures.
     * @return `true` when the backend is ready for use.
     */
    bool warmup(QString *errorMessage = nullptr);

    /**
     * @brief Transcribes a captured recording and emits a result signal.
     * @param recording Captured audio payload to transcribe.
     */
    void transcribe(const Recording &recording);

signals:
    /**
     * @brief Emitted when transcription succeeds.
     * @param text Final transcribed text.
     */
    void transcriptionReady(const QString &text);

    /**
     * @brief Emitted when transcription fails.
     * @param errorMessage Human-readable failure description.
     */
    void transcriptionFailed(const QString &errorMessage);

private:
    /// Owned transcription backend implementation.
    std::unique_ptr<TranscriptionSession> m_transcriber;
};
