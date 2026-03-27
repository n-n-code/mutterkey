#include "audio/recordingnormalizer.h"

#include "audio/recording.h"
#include "transcription/transcriptiontypes.h"

#include <QByteArray>
#include <QLoggingCategory>
#include <QString>
#include <QtCore/qstring.h>
#include <QtCore/qtypes.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace {

Q_LOGGING_CATEGORY(normalizerLog, "mutterkey.audio.normalizer")

//
// PCM helpers
//

constexpr int kWhisperSampleRate = 16000;
constexpr qsizetype kSampleBytes = static_cast<qsizetype>(sizeof(qint16));

qint16 readLittleEndianSample(const QByteArray &pcmData, qsizetype byteOffset)
{
    const auto firstByte = static_cast<quint16>(static_cast<unsigned char>(pcmData.at(byteOffset)));
    const auto secondByte = static_cast<quint16>(static_cast<unsigned char>(pcmData.at(byteOffset + 1)));
    const auto combined =
        static_cast<quint16>(static_cast<quint32>(firstByte) | (static_cast<quint32>(secondByte) << 8U));
    return static_cast<qint16>(combined);
}

std::vector<float> decodeMono(const Recording &recording)
{
    const int channels = recording.format.channelCount();
    const qsizetype bytesPerFrame = kSampleBytes * static_cast<qsizetype>(channels);
    const qsizetype frameCount = recording.pcmData.size() / bytesPerFrame;

    std::vector<float> mono;
    mono.reserve(static_cast<size_t>(frameCount));

    for (qsizetype frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        float mixedSample = 0.0f;
        for (int channel = 0; channel < channels; ++channel) {
            // Qt audio capture gives us interleaved little-endian PCM frames.
            const qsizetype byteOffset =
                (frameIndex * bytesPerFrame) + (static_cast<qsizetype>(channel) * kSampleBytes);
            const auto sample = readLittleEndianSample(recording.pcmData, byteOffset);
            mixedSample += static_cast<float>(sample) / 32768.0f;
        }
        mono.push_back(mixedSample / static_cast<float>(channels));
    }

    return mono;
}

std::vector<float> resampleLinear(const std::vector<float> &samples, int inputSampleRate)
{
    if (samples.empty() || inputSampleRate <= 0) {
        return {};
    }

    if (inputSampleRate == kWhisperSampleRate) {
        return samples;
    }

    const double ratio = static_cast<double>(kWhisperSampleRate) / static_cast<double>(inputSampleRate);
    const auto outputSampleCount = std::max<size_t>(
        1,
        static_cast<size_t>(std::llround(static_cast<double>(samples.size()) * ratio)));

    std::vector<float> resampled;
    resampled.reserve(outputSampleCount);

    // Whisper expects 16 kHz mono float samples; linear interpolation is enough for the
    // simple format adaptation this app needs before inference.
    for (size_t index = 0; index < outputSampleCount; ++index) {
        const double sourcePosition = static_cast<double>(index) / ratio;
        const auto leftIndex = static_cast<size_t>(std::floor(sourcePosition));
        const size_t rightIndex = std::min(leftIndex + 1, samples.size() - 1);
        const double blend = sourcePosition - static_cast<double>(leftIndex);
        const float leftSample = samples.at(leftIndex);
        const float rightSample = samples.at(rightIndex);
        const auto interpolated = static_cast<float>(((1.0 - blend) * leftSample) + (blend * rightSample));
        resampled.push_back(interpolated);
    }

    return resampled;
}

} // namespace

bool RecordingNormalizer::normalizeForRuntime(const Recording &recording,
                                              NormalizedAudio *normalizedAudio,
                                              QString *errorMessage) const
{
    if (normalizedAudio != nullptr) {
        *normalizedAudio = {};
    }

    if (!recording.isValid()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Recording is empty");
        }
        return false;
    }

    if (recording.format.sampleFormat() != QAudioFormat::Int16) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Embedded Whisper only supports 16-bit PCM capture");
        }
        return false;
    }

    if (recording.format.channelCount() <= 0 || recording.format.sampleRate() <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Recording format is invalid");
        }
        return false;
    }

    const qsizetype bytesPerFrame = kSampleBytes * static_cast<qsizetype>(recording.format.channelCount());
    if (bytesPerFrame <= 0 || recording.pcmData.size() < bytesPerFrame) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Recording does not contain complete PCM frames");
        }
        return false;
    }

    if ((recording.pcmData.size() % bytesPerFrame) != 0) {
        qCWarning(normalizerLog) << "PCM data size is not aligned to full frames, truncating tail bytes";
    }

    NormalizedAudio result;
    result.sampleRate = kWhisperSampleRate;
    result.channels = 1;
    // The audio path stays explicit: decode capture PCM first, then resample only if the
    // input device was not already producing Whisper's preferred format.
    result.samples = resampleLinear(decodeMono(recording), recording.format.sampleRate());

    if (!result.isValid()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to normalize audio for embedded Whisper");
        }
        return false;
    }

    if (normalizedAudio != nullptr) {
        *normalizedAudio = std::move(result);
    }
    return true;
}
