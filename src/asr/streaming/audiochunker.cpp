#include "asr/streaming/audiochunker.h"

#include <algorithm>
#include <cstddef>

namespace {

constexpr int kChunkDurationMs = 200;

} // namespace

bool AudioChunker::chunkAudio(const NormalizedAudio &audio, std::vector<AudioChunk> *chunks, QString *errorMessage) const
{
    if (chunks == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Internal error: missing audio chunk output");
        }
        return false;
    }

    chunks->clear();

    if (!audio.isValid()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Normalized audio is empty");
        }
        return false;
    }

    if (audio.sampleRate <= 0 || audio.channels != 1) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Normalized audio format is invalid");
        }
        return false;
    }

    const int chunkFrames = std::max(1, (audio.sampleRate * kChunkDurationMs) / 1000);
    chunks->reserve((static_cast<int>(audio.samples.size()) + chunkFrames - 1) / chunkFrames);

    std::int64_t streamOffsetFrames = 0;
    for (std::size_t startIndex = 0; startIndex < audio.samples.size(); startIndex += static_cast<std::size_t>(chunkFrames)) {
        const std::size_t endIndex =
            std::min(startIndex + static_cast<std::size_t>(chunkFrames), audio.samples.size());

        AudioChunk chunk;
        chunk.sampleRate = audio.sampleRate;
        chunk.channels = audio.channels;
        chunk.streamOffsetFrames = streamOffsetFrames;
        chunk.samples.assign(audio.samples.begin() + static_cast<std::ptrdiff_t>(startIndex),
                             audio.samples.begin() + static_cast<std::ptrdiff_t>(endIndex));
        chunks->push_back(std::move(chunk));
        streamOffsetFrames += static_cast<std::int64_t>(endIndex - startIndex);
    }

    return !chunks->empty();
}
