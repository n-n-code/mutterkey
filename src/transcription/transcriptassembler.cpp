#include "transcription/transcriptassembler.h"

void TranscriptAssembler::reset()
{
    m_finalTranscript.clear();
    m_latestPartial.clear();
}

void TranscriptAssembler::applyUpdate(const TranscriptUpdate &update)
{
    for (const TranscriptEvent &event : update.events) {
        const QString trimmedText = event.text.trimmed();
        if (trimmedText.isEmpty()) {
            continue;
        }

        if (event.kind == TranscriptEventKind::Final) {
            if (!m_finalTranscript.isEmpty()) {
                m_finalTranscript += QLatin1Char(' ');
            }
            m_finalTranscript += trimmedText;
            m_latestPartial.clear();
            continue;
        }

        m_latestPartial = trimmedText;
    }
}

QString TranscriptAssembler::finalTranscript() const
{
    return m_finalTranscript.trimmed();
}
