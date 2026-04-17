#include "testutil/pcmwavreader.h"

#include <QByteArray>
#include <QDataStream>
#include <QFile>

#include <array>
#include <cstdint>
#include <string_view>

namespace {

void setError(QString *out, const QString &message)
{
    if (out != nullptr) {
        *out = message;
    }
}

bool readChunkTag(QDataStream &stream, std::array<char, 4> &tag)
{
    return stream.readRawData(tag.data(), static_cast<int>(tag.size())) == static_cast<int>(tag.size());
}

bool tagEquals(const std::array<char, 4> &tag, const char *expected)
{
    const std::string_view expectedTag(expected, tag.size());
    for (std::size_t i = 0; i < tag.size(); ++i) {
        if (tag.at(i) != expectedTag.at(i)) {
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<std::vector<float>> readMono16kHzPcmWav(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Failed to open WAV: %1").arg(file.errorString()));
        return std::nullopt;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);

    std::array<char, 4> riff{};
    std::uint32_t riffSize = 0;
    std::array<char, 4> wave{};
    if (!readChunkTag(stream, riff)) {
        setError(error, QStringLiteral("Truncated RIFF tag in %1").arg(path));
        return std::nullopt;
    }
    stream >> riffSize;
    if (stream.status() != QDataStream::Ok || !readChunkTag(stream, wave)) {
        setError(error, QStringLiteral("Truncated RIFF header in %1").arg(path));
        return std::nullopt;
    }
    if (!tagEquals(riff, "RIFF") || !tagEquals(wave, "WAVE")) {
        setError(error, QStringLiteral("Not a RIFF/WAVE file: %1").arg(path));
        return std::nullopt;
    }

    bool sawFormat = false;
    while (!stream.atEnd()) {
        std::array<char, 4> chunkId{};
        std::uint32_t chunkSize = 0;
        if (!readChunkTag(stream, chunkId)) {
            break;
        }
        stream >> chunkSize;
        if (stream.status() != QDataStream::Ok) {
            break;
        }

        if (tagEquals(chunkId, "fmt ")) {
            if (chunkSize < 16) {
                setError(error, QStringLiteral("fmt chunk too small in %1").arg(path));
                return std::nullopt;
            }
            std::uint16_t audioFormat = 0;
            std::uint16_t numChannels = 0;
            std::uint32_t sampleRate = 0;
            std::uint32_t byteRate = 0;
            std::uint16_t blockAlign = 0;
            std::uint16_t bitsPerSample = 0;
            stream >> audioFormat >> numChannels >> sampleRate >> byteRate >> blockAlign >> bitsPerSample;
            if (stream.status() != QDataStream::Ok) {
                setError(error, QStringLiteral("Truncated fmt chunk in %1").arg(path));
                return std::nullopt;
            }
            const qint64 fmtExtra = static_cast<qint64>(chunkSize) - 16;
            if (fmtExtra > 0 && stream.skipRawData(static_cast<int>(fmtExtra)) != fmtExtra) {
                setError(error, QStringLiteral("Failed to skip fmt tail in %1").arg(path));
                return std::nullopt;
            }
            if (audioFormat != 1) {
                setError(error, QStringLiteral("Unsupported WAV format (not PCM) in %1").arg(path));
                return std::nullopt;
            }
            if (numChannels != 1) {
                setError(error, QStringLiteral("Unsupported channel count %1 in %2").arg(numChannels).arg(path));
                return std::nullopt;
            }
            if (sampleRate != 16000) {
                setError(error, QStringLiteral("Unsupported sample rate %1 Hz in %2").arg(sampleRate).arg(path));
                return std::nullopt;
            }
            if (bitsPerSample != 16) {
                setError(error, QStringLiteral("Unsupported bit depth %1 in %2").arg(bitsPerSample).arg(path));
                return std::nullopt;
            }
            sawFormat = true;
        } else if (tagEquals(chunkId, "data")) {
            if (!sawFormat) {
                setError(error, QStringLiteral("data chunk precedes fmt in %1").arg(path));
                return std::nullopt;
            }
            const qint64 sampleCount = static_cast<qint64>(chunkSize) / 2;
            std::vector<float> samples;
            samples.resize(static_cast<std::size_t>(sampleCount));
            constexpr float kInt16Scale = 1.0F / 32768.0F;
            for (qint64 i = 0; i < sampleCount; ++i) {
                std::int16_t sample = 0;
                stream >> sample;
                if (stream.status() != QDataStream::Ok) {
                    setError(error, QStringLiteral("Truncated data chunk in %1").arg(path));
                    return std::nullopt;
                }
                samples.at(static_cast<std::size_t>(i)) = static_cast<float>(sample) * kInt16Scale;
            }
            // data chunks are byte-aligned to even sizes; skip the pad byte if present.
            if ((chunkSize % 2) != 0) {
                stream.skipRawData(1);
            }
            return samples;
        } else {
            const qint64 advance = static_cast<qint64>(chunkSize) + ((chunkSize % 2) != 0 ? 1 : 0);
            if (stream.skipRawData(static_cast<int>(advance)) != advance) {
                break;
            }
        }
    }

    setError(error, QStringLiteral("No data chunk found in %1").arg(path));
    return std::nullopt;
}
