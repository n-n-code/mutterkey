#pragma once

#include <QString>

#include <vector>

struct TranscriptionResult {
    bool success = false;
    QString text;
    QString error;
};

struct NormalizedAudio {
    std::vector<float> samples;
    int sampleRate = 16000;
    int channels = 1;

    [[nodiscard]] bool isValid() const { return !samples.empty(); }
};
