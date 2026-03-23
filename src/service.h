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

class MutterkeyService final : public QObject
{
    Q_OBJECT

public:
    explicit MutterkeyService(const AppConfig &config, QClipboard *clipboard, QObject *parent = nullptr);
    ~MutterkeyService() override;

    Q_DISABLE_COPY_MOVE(MutterkeyService)

    bool start(QString *errorMessage = nullptr);
    void stop();
    [[nodiscard]] QJsonObject diagnostics() const;
    bool invokeShortcut(QString *errorMessage = nullptr);

signals:
    void transcriptionReady(const QString &text);
    void transcriptionFailed(const QString &errorMessage);

private slots:
    void onShortcutPressed();
    void onShortcutReleased();
    void onTranscriptionReady(const QString &text);
    void onTranscriptionFailed(const QString &errorMessage);

private:
    bool startTranscriptionWorker(QString *errorMessage = nullptr);
    void stopTranscriptionWorker();
    void transcribeInBackground(Recording recording);

    AppConfig m_config;
    AudioRecorder m_audioRecorder;
    ClipboardWriter m_clipboardWriter;
    HotkeyManager m_hotkeyManager;
    QThread m_transcriptionThread;
    TranscriptionWorker *m_transcriptionWorker = nullptr;
    bool m_running = false;
    int m_recordingsStarted = 0;
    int m_recordingsCompleted = 0;
    int m_transcriptionsCompleted = 0;
};
