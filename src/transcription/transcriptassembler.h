#pragma once

#include "transcription/transcriptiontypes.h"

#include <QString>

/**
 * @file
 * @brief Helpers for assembling clipboard-friendly text from transcript events.
 */

/**
 * @brief Collects streaming transcript events into a final user-facing transcript.
 */
class TranscriptAssembler final
{
public:
    /**
     * @brief Resets any accumulated transcript state.
     */
    void reset();

    /**
     * @brief Applies a streaming update to the assembled transcript state.
     * @param update Session update containing zero or more events.
     */
    void applyUpdate(const TranscriptUpdate &update);

    /**
     * @brief Returns the current assembled final transcript.
     * @return Clipboard-friendly joined final transcript text.
     */
    [[nodiscard]] QString finalTranscript() const;

private:
    QString m_finalTranscript;
    QString m_latestPartial;
};
