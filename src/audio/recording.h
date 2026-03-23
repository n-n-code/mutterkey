#pragma once

#include <QAudioFormat>
#include <QByteArray>

struct Recording {
    QByteArray pcmData;
    QAudioFormat format;
    double durationSeconds = 0.0;

    // A recording is only useful once capture produced both payload bytes and a valid
    // format description for later normalization/transcription.
    [[nodiscard]] bool isValid() const { return !pcmData.isEmpty() && format.isValid(); }
};
