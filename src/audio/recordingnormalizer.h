#pragma once

#include "audio/recording.h"
#include "transcription/transcriptiontypes.h"

#include <QString>

class RecordingNormalizer final
{
public:
    bool normalizeForWhisper(const Recording &recording, NormalizedAudio *normalizedAudio, QString *errorMessage = nullptr) const;
};
