#pragma once

#include "audio/recording.h"
#include "config.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QElapsedTimer>
#include <QIODevice>
#include <QObject>

class QAudioSource;

/**
 * @file
 * @brief Audio capture primitives for push-to-talk recording.
 */

/**
 * @brief QIODevice sink that accumulates raw captured PCM bytes in memory.
 */
class AudioBufferDevice final : public QIODevice
{
    Q_OBJECT

public:
    /**
     * @brief Creates an empty in-memory audio sink.
     * @param parent Optional QObject parent.
     */
    explicit AudioBufferDevice(QObject *parent = nullptr);
    ~AudioBufferDevice() override = default;

    Q_DISABLE_COPY_MOVE(AudioBufferDevice)

    /**
     * @brief Discards all buffered audio bytes.
     */
    void clear();

    /**
     * @brief Moves the buffered audio payload out of the device.
     * @return Captured PCM bytes accumulated since the last clear or take.
     */
    [[nodiscard]] QByteArray takeBuffer();

protected:
    /**
     * @brief Audio capture is write-only for this device.
     * @return Always returns `-1`.
     */
    qint64 readData(char *data, qint64 maxSize) override;

    /**
     * @brief Appends captured audio bytes to the in-memory buffer.
     * @param data Source byte range from Qt Multimedia.
     * @param maxSize Number of bytes available.
     * @return Number of bytes consumed.
     */
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    /// Raw PCM payload accumulated during recording.
    QByteArray m_buffer;
};

/**
 * @brief Captures microphone audio into a `Recording` value object.
 *
 * The recorder owns the active `QAudioSource` while recording. The returned
 * `Recording` preserves the device-selected format so later normalization can
 * convert it into Whisper's required mono 16 kHz float input.
 */
class AudioRecorder final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates a recorder with a fixed audio configuration snapshot.
     * @param config Audio capture preferences copied into the recorder.
     * @param parent Optional QObject parent.
     */
    explicit AudioRecorder(AudioConfig config, QObject *parent = nullptr);
    ~AudioRecorder() override = default;

    Q_DISABLE_COPY_MOVE(AudioRecorder)

    /**
     * @brief Starts audio capture on the resolved input device.
     * @param errorMessage Optional output for startup failures.
     * @return `true` when capture started successfully.
     */
    bool start(QString *errorMessage = nullptr);

    /**
     * @brief Stops capture and returns the recorded payload and format metadata.
     * @return Captured recording. Invalid when capture was not active or failed.
     */
    Recording stop();

    /**
     * @brief Reports whether microphone capture is currently active.
     * @return `true` when a `QAudioSource` is running.
     */
    [[nodiscard]] bool isRecording() const;

private:
    /**
     * @brief Selects the requested or default input device.
     * @return Resolved Qt audio device descriptor.
     */
    [[nodiscard]] QAudioDevice resolveDevice() const;

    /**
     * @brief Resolves a compatible audio format for the selected device.
     * @param device Device to configure.
     * @param errorMessage Optional output for unsupported format failures.
     * @return Active format chosen for capture.
     */
    QAudioFormat resolveFormat(const QAudioDevice &device, QString *errorMessage) const;

    /// Immutable audio capture preferences.
    AudioConfig m_config;
    /// In-memory sink used by the active audio source.
    AudioBufferDevice m_buffer;
    /// Owned Qt audio source while recording is active.
    QAudioSource *m_audioSource = nullptr;
    /// Format actually used by the active recording session.
    QAudioFormat m_activeFormat;
    /// Measures recording duration for minimum-length validation and metadata.
    QElapsedTimer m_elapsed;
};
