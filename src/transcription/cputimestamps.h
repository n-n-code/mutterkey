#pragma once

#include "transcription/cputokenizer.h"
#include "transcription/transcriptiontypes.h"

#include <cstdint>
#include <span>

struct CpuReferenceExecutionMetadata;

/**
 * @file
 * @brief Timestamp helpers for native CPU transcript events.
 */

/**
 * @brief Builds one final transcript event for a decoded utterance.
 * @param text Final transcript text.
 * @param tokens Decoder tokens emitted for the utterance.
 * @param samples Buffered utterance samples.
 * @param sampleRate Buffered utterance sample rate.
 * @param execution Optional native execution metadata used for timestamp-token interpretation.
 * @return Final transcript event with coarse utterance timestamps.
 */
[[nodiscard]] TranscriptEvent buildCpuFinalTranscriptEvent(const QString &text,
                                                           std::span<const CpuDecodedToken> tokens,
                                                           std::span<const float> samples,
                                                           int sampleRate,
                                                           const CpuReferenceExecutionMetadata *execution = nullptr);
