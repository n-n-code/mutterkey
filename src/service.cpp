#include "service.h"

#include <QLoggingCategory>
#include <QMetaObject>

namespace {

Q_STATIC_LOGGING_CATEGORY(serviceLog, "mutterkey.service")

} // namespace

MutterkeyService::MutterkeyService(const AppConfig &config, QClipboard *clipboard, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_audioRecorder(config.audio, this)
    , m_clipboardWriter(clipboard, this)
    , m_hotkeyManager(config.shortcut, this)
{
    connect(&m_hotkeyManager, &HotkeyManager::shortcutPressed, this, &MutterkeyService::onShortcutPressed);
    connect(&m_hotkeyManager, &HotkeyManager::shortcutReleased, this, &MutterkeyService::onShortcutReleased);
}

MutterkeyService::~MutterkeyService()
{
    stop();
}

bool MutterkeyService::start(QString *errorMessage)
{
    if (!startTranscriptionWorker(errorMessage)) {
        return false;
    }

    // Keep result handling on the service thread even though transcription runs on its
    // own worker thread, so clipboard updates and counters stay serialized here.
    connect(this, &MutterkeyService::transcriptionReady, this, &MutterkeyService::onTranscriptionReady, Qt::UniqueConnection);
    connect(this, &MutterkeyService::transcriptionFailed, this, &MutterkeyService::onTranscriptionFailed, Qt::UniqueConnection);

    if (!m_hotkeyManager.registerShortcut(errorMessage)) {
        stopTranscriptionWorker();
        return false;
    }

    m_running = true;
    qCInfo(serviceLog) << "Clipboard backend:" << m_clipboardWriter.backendName();
    qCInfo(serviceLog) << "Transcriber backend:" << m_transcriptionWorker->backendName();
    return true;
}

void MutterkeyService::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;
    if (m_audioRecorder.isRecording()) {
        m_audioRecorder.stop();
    }
    m_hotkeyManager.unregisterShortcut();
    stopTranscriptionWorker();
}

QJsonObject MutterkeyService::diagnostics() const
{
    QJsonObject object;
    object.insert(QStringLiteral("hotkey"), m_hotkeyManager.diagnostics());
    object.insert(QStringLiteral("recorder_running"), m_audioRecorder.isRecording());
    object.insert(QStringLiteral("recordings_started"), m_recordingsStarted);
    object.insert(QStringLiteral("recordings_completed"), m_recordingsCompleted);
    object.insert(QStringLiteral("clipboard_backend"), m_clipboardWriter.backendName());
    object.insert(QStringLiteral("transcriptions_completed"), m_transcriptionsCompleted);
    object.insert(QStringLiteral("transcriber_backend"),
                  m_transcriptionWorker != nullptr ? m_transcriptionWorker->backendName() : QStringLiteral("unconfigured"));
    return object;
}

bool MutterkeyService::invokeShortcut(QString *errorMessage)
{
    return m_hotkeyManager.invokeShortcut(errorMessage);
}

void MutterkeyService::onShortcutPressed()
{
    QString errorMessage;
    if (!m_audioRecorder.start(&errorMessage)) {
        if (m_audioRecorder.isRecording()) {
            qCInfo(serviceLog) << "Ignored press because the recorder is already running";
        } else if (!errorMessage.isEmpty()) {
            qCWarning(serviceLog) << "Could not start recording:" << errorMessage;
        }
        return;
    }

    ++m_recordingsStarted;
    qCInfo(serviceLog) << "Recording started";
}

void MutterkeyService::onShortcutReleased()
{
    const Recording recording = m_audioRecorder.stop();
    if (!recording.isValid()) {
        qCInfo(serviceLog) << "Ignored release because there is no active recording";
        return;
    }

    ++m_recordingsCompleted;
    qCInfo(serviceLog) << "Recording stopped with"
                       << recording.durationSeconds
                       << "seconds of audio and"
                       << recording.pcmData.size()
                       << "bytes captured";
    if (recording.durationSeconds < m_config.audio.minimumSeconds) {
        qCInfo(serviceLog) << "Dropped recording below minimum duration threshold";
        return;
    }

    transcribeInBackground(recording);
}

void MutterkeyService::onTranscriptionReady(const QString &text)
{
    if (!m_running || text.trimmed().isEmpty()) {
        return;
    }

    const QString trimmedText = text.trimmed();
    qCInfo(serviceLog) << "Transcription ready with" << trimmedText.size() << "characters";

    ++m_transcriptionsCompleted;
    if (!m_clipboardWriter.copy(trimmedText)) {
        qCWarning(serviceLog) << "Clipboard update appears to have failed";
    }
}

void MutterkeyService::onTranscriptionFailed(const QString &errorMessage)
{
    qCWarning(serviceLog) << "Transcription failed:" << errorMessage;
}

void MutterkeyService::transcribeInBackground(Recording recording)
{
    if (m_transcriptionWorker == nullptr) {
        emit transcriptionFailed(QStringLiteral("Transcription worker is not running"));
        return;
    }

    qCInfo(serviceLog) << "Transcribing" << recording.durationSeconds << "seconds of audio";
    // Move the captured recording into the worker-thread invocation so we do not keep
    // another owner of the PCM payload alive on the service thread.
    QMetaObject::invokeMethod(m_transcriptionWorker,
                              [worker = m_transcriptionWorker, recording = std::move(recording)]() mutable {
                                  worker->transcribe(recording);
                              },
                              Qt::QueuedConnection);
}

bool MutterkeyService::startTranscriptionWorker(QString *errorMessage)
{
    if (m_transcriptionWorker != nullptr) {
        return true;
    }

    // The worker owns the transcriber backend but lives on a dedicated thread so hotkey
    // and recorder callbacks do not block on model initialization or inference.
    m_transcriptionWorker = new TranscriptionWorker(m_config.transcriber);
    m_transcriptionWorker->moveToThread(&m_transcriptionThread);

    connect(&m_transcriptionThread, &QThread::finished, m_transcriptionWorker, &QObject::deleteLater);
    connect(m_transcriptionWorker, &TranscriptionWorker::transcriptionReady, this, &MutterkeyService::transcriptionReady);
    connect(m_transcriptionWorker, &TranscriptionWorker::transcriptionFailed, this, &MutterkeyService::transcriptionFailed);

    m_transcriptionThread.start();

    if (m_config.transcriber.warmupOnStart) {
        bool warmupOk = false;
        QString warmupError;
        // Warmup has to run on the worker thread because that thread also owns the
        // transcriber object for the remainder of the process lifetime.
        QMetaObject::invokeMethod(m_transcriptionWorker,
                                  [this, &warmupOk, &warmupError]() {
                                      warmupOk = m_transcriptionWorker->warmup(&warmupError);
                                  },
                                  Qt::BlockingQueuedConnection);
        if (!warmupOk) {
            if (errorMessage != nullptr) {
                *errorMessage = warmupError;
            }
            stopTranscriptionWorker();
            return false;
        }
    }

    return true;
}

void MutterkeyService::stopTranscriptionWorker()
{
    if (m_transcriptionWorker == nullptr) {
        return;
    }

    disconnect(m_transcriptionWorker, nullptr, this, nullptr);

    // Let the thread drain queued transcription work before dropping our raw pointer.
    m_transcriptionThread.quit();
    m_transcriptionThread.wait();
    m_transcriptionWorker = nullptr;
}
