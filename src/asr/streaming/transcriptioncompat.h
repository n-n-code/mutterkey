#pragma once

#include "audio/recording.h"
#include "asr/runtime/transcriptionengine.h"
#include "asr/runtime/transcriptiontypes.h"

class RecordingNormalizer;

/**
 * @file
 * @brief Compatibility helpers that run one-shot recordings through the streaming runtime path.
 */

/**
 * @brief Transcribes a captured recording through the chunked session contract.
 * @param session Live mutable streaming session.
 * @param recording Captured audio payload to normalize and stream.
 * @param normalizer Format conversion helper.
 * @return One-shot transcription result assembled from final transcript events.
 */
[[nodiscard]] TranscriptionResult transcribeRecordingViaStreaming(TranscriptionSession &session,
                                                                  const Recording &recording,
                                                                  const RecordingNormalizer &normalizer);
