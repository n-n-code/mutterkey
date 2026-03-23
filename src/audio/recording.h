#pragma once

#include <QAudioFormat>
#include <QByteArray>

/**
 * @file
 * @brief Shared recorded-audio payload passed between capture and transcription.
 */

/**
 * @brief Immutable-style value object holding one captured audio segment.
 */
struct Recording {
    /// Raw PCM payload produced by Qt Multimedia.
    QByteArray pcmData;
    /// Device-selected format describing the PCM payload.
    QAudioFormat format;
    /// Observed recording duration in seconds.
    double durationSeconds = 0.0;

    /**
     * @brief Reports whether the recording contains usable audio payload.
     *
     * A recording is only useful once capture produced both payload bytes and a
     * valid format description for later normalization and transcription.
     *
     * @return `true` when both payload and format metadata are present.
     */
    [[nodiscard]] bool isValid() const { return !pcmData.isEmpty() && format.isValid(); }
};
