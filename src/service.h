#pragma once

#include "audio/audiorecorder.h"
#include "clipboardwriter.h"
#include "config.h"
#include "hotkeymanager.h"
#include "transcription/transcriptionworker.h"

#include <QJsonObject>
#include <QObject>
#include <QThread>

class QClipboard;

/**
 * @file
 * @brief Background service that wires hotkey, audio capture, transcription,
 * and clipboard delivery together.
 */

/**
 * @brief Coordinates the daemon-mode push-to-talk workflow.
 *
 * The service owns the recorder, clipboard writer, hotkey manager, and a
 * dedicated transcription thread. The worker object itself lives on the
 * transcription thread, while this service stays on the main thread and
 * coordinates cross-thread requests through Qt signal and slot delivery.
 */
class MutterkeyService final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates the service with a fixed runtime configuration.
     * @param config Startup configuration snapshot copied into the service.
     * @param clipboard Non-owning pointer to the application clipboard.
     * @param parent Optional QObject parent.
     */
    explicit MutterkeyService(const AppConfig &config, QClipboard *clipboard, QObject *parent = nullptr);

    /**
     * @brief Stops background work and joins the transcription thread.
     */
    ~MutterkeyService() override;

    Q_DISABLE_COPY_MOVE(MutterkeyService)

    /**
     * @brief Starts the hotkey, worker thread, and optional backend warmup.
     * @param errorMessage Optional output for startup failures.
     * @return `true` when the service reached a running state.
     */
    bool start(QString *errorMessage = nullptr);

    /**
     * @brief Stops recording, hotkey registration, and background transcription.
     */
    void stop();

    /**
     * @brief Returns current service diagnostics for `diagnose` mode.
     * @return JSON object with runtime counters and subsystem state.
     */
    [[nodiscard]] QJsonObject diagnostics() const;

    /**
     * @brief Invokes the registered shortcut action for diagnostics.
     * @param errorMessage Optional output for invocation failures.
     * @return `true` when invocation was dispatched.
     */
    bool invokeShortcut(QString *errorMessage = nullptr);

signals:
    /**
     * @brief Emitted when a transcription result has been copied successfully.
     * @param text Final transcribed text.
     */
    void transcriptionReady(const QString &text);

    /**
     * @brief Emitted when recording or transcription fails.
     * @param errorMessage Human-readable failure description.
     */
    void transcriptionFailed(const QString &errorMessage);

private slots:
    /**
     * @brief Starts audio capture when the shortcut is pressed.
     */
    void onShortcutPressed();

    /**
     * @brief Stops capture and dispatches transcription when the shortcut is released.
     */
    void onShortcutReleased();

    /**
     * @brief Writes a successful transcription to the clipboard and updates counters.
     * @param text Final transcribed text.
     */
    void onTranscriptionReady(const QString &text);

    /**
     * @brief Handles transcription failures reported by the worker thread.
     * @param errorMessage Human-readable failure description.
     */
    void onTranscriptionFailed(const QString &errorMessage);

private:
    /**
     * @brief Creates and starts the background transcription worker.
     * @param errorMessage Optional output for startup failures.
     * @return `true` when the worker thread is ready to accept work.
     */
    bool startTranscriptionWorker(QString *errorMessage = nullptr);

    /**
     * @brief Stops the background transcription worker and joins its thread.
     */
    void stopTranscriptionWorker();

    /**
     * @brief Queues a captured recording for background transcription.
     * @param recording Captured audio payload moved into the queued request.
     */
    void transcribeInBackground(Recording recording);

    /// Immutable runtime configuration snapshot.
    AppConfig m_config;
    /// Main-thread recorder used while the push-to-talk shortcut is held.
    AudioRecorder m_audioRecorder;
    /// Clipboard delivery helper for successful transcription results.
    ClipboardWriter m_clipboardWriter;
    /// KDE global shortcut registration and diagnostics helper.
    HotkeyManager m_hotkeyManager;
    /// Dedicated thread that hosts the transcription worker object.
    QThread m_transcriptionThread;
    /// Non-owning pointer to the worker after it is moved to `m_transcriptionThread`.
    TranscriptionWorker *m_transcriptionWorker = nullptr;
    /// Tracks whether start() completed successfully and stop() is required.
    bool m_running = false;
    /// Number of recordings started since service startup.
    int m_recordingsStarted = 0;
    /// Number of recordings stopped and handed off for transcription.
    int m_recordingsCompleted = 0;
    /// Number of successful transcriptions observed by the service.
    int m_transcriptionsCompleted = 0;
};
