#include "asr/nativecpu/cputimestamps.h"

#include "asr/nativecpu/cpureferencemodel.h"

#include <algorithm>
#include <utility>

namespace {

std::pair<std::int64_t, std::int64_t> timestampRangeFromTokens(std::span<const CpuDecodedToken> tokens,
                                                               const CpuReferenceExecutionMetadata &execution,
                                                               std::int64_t durationMs)
{
    constexpr std::int64_t kTimestampStepMs = 20;

    int firstTimestamp = -1;
    int lastTimestamp = -1;
    for (const CpuDecodedToken &token : tokens) {
        if (token.kind != CpuDecodedTokenKind::Timestamp) {
            continue;
        }
        if (firstTimestamp < 0) {
            firstTimestamp = token.id;
        }
        lastTimestamp = token.id;
    }

    if (firstTimestamp < 0 || lastTimestamp < firstTimestamp) {
        return {0, durationMs};
    }

    const std::int64_t startMs = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(firstTimestamp - execution.timestampTokenStartId) * kTimestampStepMs,
        0,
        durationMs);
    const std::int64_t endMs = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(lastTimestamp - execution.timestampTokenStartId + 1) * kTimestampStepMs,
        startMs + 1,
        durationMs);
    return {startMs, endMs};
}

} // namespace

TranscriptEvent buildCpuFinalTranscriptEvent(const QString &text,
                                             std::span<const CpuDecodedToken> tokens,
                                             std::span<const float> samples,
                                             int sampleRate,
                                             const CpuReferenceExecutionMetadata *execution)
{
    const auto durationMs = std::max<std::int64_t>(
        1,
        sampleRate > 0 ? static_cast<std::int64_t>((static_cast<double>(samples.size()) * 1000.0) / sampleRate) : 1);
    const std::pair<std::int64_t, std::int64_t> eventRange =
        execution != nullptr && execution->timestampMode == QStringLiteral("timestamp-token-v1")
        ? timestampRangeFromTokens(tokens, *execution, durationMs)
        : std::pair<std::int64_t, std::int64_t>{0, durationMs};
    return TranscriptEvent{
        .kind = TranscriptEventKind::Final,
        .text = text,
        .startMs = eventRange.first,
        .endMs = eventRange.second,
    };
}
