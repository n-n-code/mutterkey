#include "audio/audiorecorder.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QLoggingCategory>
#include <QMediaDevices>

#include <utility>

namespace {

Q_STATIC_LOGGING_CATEGORY(audioLog, "mutterkey.audio")

} // namespace

AudioBufferDevice::AudioBufferDevice(QObject *parent)
    : QIODevice(parent)
{
}

void AudioBufferDevice::clear()
{
    m_buffer.clear();
}

QByteArray AudioBufferDevice::takeBuffer()
{
    QByteArray result = m_buffer;
    m_buffer.clear();
    return result;
}

qint64 AudioBufferDevice::readData(char *, qint64)
{
    return -1;
}

qint64 AudioBufferDevice::writeData(const char *data, qint64 maxSize)
{
    m_buffer.append(data, static_cast<qsizetype>(maxSize));
    return maxSize;
}

AudioRecorder::AudioRecorder(AudioConfig config, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_buffer(this)
{
}

bool AudioRecorder::start(QString *errorMessage)
{
    if (m_audioSource != nullptr) {
        return false;
    }

    const QAudioDevice device = resolveDevice();
    if (device.isNull()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No audio input device available");
        }
        return false;
    }

    const QAudioFormat format = resolveFormat(device, errorMessage);
    if (!format.isValid()) {
        return false;
    }

    m_buffer.clear();
    if (!m_buffer.isOpen()) {
        m_buffer.open(QIODevice::WriteOnly);
    }

    m_audioSource = new QAudioSource(device, format, this);
    m_audioSource->setBufferSize(format.bytesForDuration(250000));
    m_audioSource->start(&m_buffer);
    if (m_audioSource->error() != QtAudio::NoError) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to start audio source");
        }
        delete m_audioSource;
        m_audioSource = nullptr;
        m_buffer.close();
        return false;
    }

    m_activeFormat = format;
    m_elapsed.restart();
    qCInfo(audioLog) << "Audio capture started with format"
                     << format.sampleRate()
                     << "Hz"
                     << format.channelCount()
                     << "channels";
    return true;
}

Recording AudioRecorder::stop()
{
    Recording recording;
    if (m_audioSource == nullptr) {
        return recording;
    }

    m_audioSource->stop();
    m_buffer.close();

    recording.pcmData = m_buffer.takeBuffer();
    recording.format = m_activeFormat;
    recording.durationSeconds = static_cast<double>(m_elapsed.elapsed()) / 1000.0;

    delete m_audioSource;
    m_audioSource = nullptr;
    m_activeFormat = QAudioFormat();
    return recording;
}

bool AudioRecorder::isRecording() const
{
    return m_audioSource != nullptr;
}

QAudioDevice AudioRecorder::resolveDevice() const
{
    const QList<QAudioDevice> devices = QMediaDevices::audioInputs();
    if (devices.isEmpty()) {
        return {};
    }

    if (m_config.deviceId.isEmpty()) {
        return QMediaDevices::defaultAudioInput();
    }

    for (const QAudioDevice &device : devices) {
        if (QString::fromUtf8(device.id()) == m_config.deviceId) {
            return device;
        }
    }

    qCWarning(audioLog) << "Configured audio device not found, using default input:" << m_config.deviceId;
    return QMediaDevices::defaultAudioInput();
}

QAudioFormat AudioRecorder::resolveFormat(const QAudioDevice &device, QString *errorMessage) const
{
    QAudioFormat requested;
    requested.setSampleRate(m_config.sampleRate);
    requested.setChannelCount(m_config.channels);
    requested.setSampleFormat(QAudioFormat::Int16);

    if (device.isFormatSupported(requested)) {
        return requested;
    }

    const QAudioFormat preferred = device.preferredFormat();
    if (preferred.sampleFormat() != QAudioFormat::Int16) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected audio device does not support 16-bit PCM capture");
        }
        return {};
    }

    qCWarning(audioLog) << "Requested audio format is unsupported, falling back to preferred format";
    return preferred;
}
