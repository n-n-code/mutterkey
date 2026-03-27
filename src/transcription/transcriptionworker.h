#pragma once

#include "audio/recording.h"
#include "audio/recordingnormalizer.h"
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
     * @brief Creates a worker with a shared immutable engine.
     * @param engine Shared engine used to lazily create the live session.
     * @param parent Optional QObject parent.
     */
    explicit TranscriptionWorker(std::shared_ptr<const TranscriptionEngine> engine, QObject *parent = nullptr);
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
     * @brief Returns the active runtime capability snapshot.
     * @return Capability data for diagnostics and orchestration decisions.
     */
    [[nodiscard]] BackendCapabilities capabilities() const;

    /**
     * @brief Returns runtime diagnostics for the active backend instance.
     * @return Runtime diagnostics snapshot including the loaded model description.
     */
    [[nodiscard]] RuntimeDiagnostics runtimeDiagnostics() const;

    /**
     * @brief Eagerly initializes backend state before the first real transcription.
     * @param error Optional output for warmup failures.
     * @return `true` when the backend is ready for use.
     */
    bool warmup(RuntimeError *error = nullptr);

    /**
     * @brief Transcribes a captured recording and emits a result signal.
     * @param recording Captured audio payload to transcribe.
     */
    void transcribeRecordingCompat(const Recording &recording);

signals:
    /**
     * @brief Emitted when transcription succeeds.
     * @param text Final transcribed text.
     */
    void transcriptionReady(const QString &text);

    /**
     * @brief Emitted when transcription fails.
     * @param error Structured failure description.
     */
    void transcriptionFailed(const RuntimeError &error);

private:
    static bool shouldDiscardSession(const RuntimeError &error);

    bool ensureModel(RuntimeError *error = nullptr);
    bool ensureSession(RuntimeError *error = nullptr);

    /// Shared immutable engine used to create the live session lazily on the worker thread.
    std::shared_ptr<const TranscriptionEngine> m_engine;
    /// Shared immutable loaded model handle reused across session instances.
    std::shared_ptr<const TranscriptionModelHandle> m_model;
    /// Capability snapshot reported even before the first session exists.
    BackendCapabilities m_capabilities;
    /// Runtime diagnostics reported separately from static capabilities.
    RuntimeDiagnostics m_runtimeDiagnostics;
    /// Product-owned recorder-to-runtime audio normalization helper.
    RecordingNormalizer m_normalizer;
    /// Owned transcription backend implementation.
    std::unique_ptr<TranscriptionSession> m_transcriber;
};
