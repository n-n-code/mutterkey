#pragma once

#include "audio/recording.h"
#include "config.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QElapsedTimer>
#include <QIODevice>
#include <QObject>

class QAudioSource;

class AudioBufferDevice final : public QIODevice
{
    Q_OBJECT

public:
    explicit AudioBufferDevice(QObject *parent = nullptr);
    ~AudioBufferDevice() override = default;

    Q_DISABLE_COPY_MOVE(AudioBufferDevice)

    void clear();
    [[nodiscard]] QByteArray takeBuffer();

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    QByteArray m_buffer;
};

class AudioRecorder final : public QObject
{
    Q_OBJECT

public:
    explicit AudioRecorder(AudioConfig config, QObject *parent = nullptr);
    ~AudioRecorder() override = default;

    Q_DISABLE_COPY_MOVE(AudioRecorder)

    bool start(QString *errorMessage = nullptr);
    Recording stop();
    [[nodiscard]] bool isRecording() const;

private:
    [[nodiscard]] QAudioDevice resolveDevice() const;
    QAudioFormat resolveFormat(const QAudioDevice &device, QString *errorMessage) const;

    AudioConfig m_config;
    AudioBufferDevice m_buffer;
    QAudioSource *m_audioSource = nullptr;
    QAudioFormat m_activeFormat;
    QElapsedTimer m_elapsed;
};
