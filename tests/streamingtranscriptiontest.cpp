#include "audio/recording.h"
#include "audio/recordingnormalizer.h"
#include "asr/streaming/audiochunker.h"
#include "asr/streaming/transcriptassembler.h"
#include "asr/streaming/transcriptioncompat.h"

#include <QAudioFormat>
#include <QtTest/QTest>

#include <array>
#include <cstdint>
#include <vector>

namespace {

void appendSample(QByteArray *pcmData, qint16 sample)
{
    const auto sampleBits = static_cast<quint16>(sample);
    pcmData->append(static_cast<char>(static_cast<quint32>(sampleBits) & 0x00ffU));
    pcmData->append(static_cast<char>((static_cast<quint32>(sampleBits) >> 8U) & 0x00ffU));
}

Recording makeStereoRecording()
{
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    Recording recording;
    recording.format = format;
    recording.durationSeconds = 0.5;

    std::array<qint16, 10> stereoFrames{};
    for (std::size_t index = 0; index < stereoFrames.size(); ++index) {
        stereoFrames.at(index) = static_cast<qint16>((index % 4U) * 500U);
    }
    for (const qint16 sample : stereoFrames) {
        appendSample(&recording.pcmData, sample);
    }
    return recording;
}

class FakeStreamingSession : public TranscriptionSession
{
public:
    [[nodiscard]] QString backendName() const override
    {
        return QStringLiteral("fake");
    }

    bool warmup(RuntimeError *error) override
    {
        Q_UNUSED(error);
        return true;
    }

    [[nodiscard]] TranscriptUpdate pushAudioChunk(const AudioChunk &chunk) override
    {
        m_chunkCount += 1;
        m_frameOffsets.push_back(chunk.streamOffsetFrames);
        m_sampleCount += static_cast<int>(chunk.samples.size());
        return TranscriptUpdate{
            .events = {TranscriptEvent{
                .kind = TranscriptEventKind::Partial,
                .text = QStringLiteral("ignored partial"),
            }},
            .error = {},
        };
    }

    [[nodiscard]] TranscriptUpdate finish() override
    {
        m_finishCalled = true;
        return TranscriptUpdate{
            .events = {
                TranscriptEvent{
                    .kind = TranscriptEventKind::Final,
                    .text = QStringLiteral("hello"),
                    .startMs = 0,
                    .endMs = 100,
                },
                TranscriptEvent{
                    .kind = TranscriptEventKind::Final,
                    .text = QStringLiteral("world"),
                    .startMs = 100,
                    .endMs = 220,
                },
            },
            .error = {},
        };
    }

    [[nodiscard]] TranscriptUpdate cancel() override
    {
        m_cancelCalled = true;
        return TranscriptUpdate{
            .events = {},
            .error = RuntimeError{
                .code = RuntimeErrorCode::Cancelled,
                .message = QStringLiteral("cancelled"),
            },
        };
    }

    [[nodiscard]] int chunkCount() const
    {
        return m_chunkCount;
    }

    [[nodiscard]] int sampleCount() const
    {
        return m_sampleCount;
    }

    [[nodiscard]] bool finishCalled() const
    {
        return m_finishCalled;
    }

    [[nodiscard]] bool cancelCalled() const
    {
        return m_cancelCalled;
    }

    [[nodiscard]] const std::vector<std::int64_t> &frameOffsets() const
    {
        return m_frameOffsets;
    }

private:
    int m_chunkCount = 0;
    int m_sampleCount = 0;
    bool m_finishCalled = false;
    bool m_cancelCalled = false;
    std::vector<std::int64_t> m_frameOffsets;
};

class StreamingTranscriptionTest final : public QObject
{
    Q_OBJECT

private slots:
    void audioChunkerSplitsNormalizedAudioIntoStableOffsets();
    void audioChunkerRejectsInvalidNormalizedAudio();
    void transcriptAssemblerJoinsOnlyFinalEvents();
    void transcriptAssemblerResetClearsFinalAndPartialState();
    void compatPathStreamsChunksAndAssemblesFinalTranscript();
    void compatPathReportsNormalizationFailure();
    void compatPathReturnsChunkPushFailure();
    void compatPathReturnsFinishFailure();
    void cancelReturnsStructuredFailure();
};

} // namespace

void StreamingTranscriptionTest::audioChunkerSplitsNormalizedAudioIntoStableOffsets()
{
    // WHAT: Verify that the audio chunker produces stable chunk sizes and stream offsets.
    // HOW: Feed it normalized mono audio longer than one chunk and confirm the number of
    // chunks, each chunk size, and each chunk's starting frame offset.
    // WHY: The streaming runtime contract depends on deterministic chunk boundaries so
    // session state, transcript timing, and retries stay predictable.
    NormalizedAudio audio;
    audio.sampleRate = 16000;
    audio.channels = 1;
    audio.samples.resize(7000, 0.25f);

    const AudioChunker chunker;
    std::vector<AudioChunk> chunks;
    QString errorMessage;

    QVERIFY(chunker.chunkAudio(audio, &chunks, &errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(static_cast<int>(chunks.size()), 3);
    QCOMPARE(chunks.at(0).streamOffsetFrames, 0);
    QCOMPARE(chunks.at(1).streamOffsetFrames, 3200);
    QCOMPARE(chunks.at(2).streamOffsetFrames, 6400);
    QCOMPARE(static_cast<int>(chunks.at(0).samples.size()), 3200);
    QCOMPARE(static_cast<int>(chunks.at(1).samples.size()), 3200);
    QCOMPARE(static_cast<int>(chunks.at(2).samples.size()), 600);
}

void StreamingTranscriptionTest::audioChunkerRejectsInvalidNormalizedAudio()
{
    // WHAT: Verify that the audio chunker rejects malformed normalized-audio input.
    // HOW: Pass one payload with no samples and another with an invalid channel count, then
    // check that chunking fails with the expected validation errors.
    // WHY: Chunking sits after normalization, so it must still fail loudly on broken runtime
    // inputs instead of producing misleading offsets or empty work for the backend.
    const AudioChunker chunker;
    std::vector<AudioChunk> chunks;
    QString errorMessage;

    QVERIFY(!chunker.chunkAudio(NormalizedAudio{}, &chunks, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("empty")));

    NormalizedAudio invalidFormat;
    invalidFormat.samples = {0.25f};
    invalidFormat.sampleRate = 16000;
    invalidFormat.channels = 2;

    QVERIFY(!chunker.chunkAudio(invalidFormat, &chunks, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("invalid")));
}

void StreamingTranscriptionTest::transcriptAssemblerJoinsOnlyFinalEvents()
{
    // WHAT: Verify that the transcript assembler keeps only final transcript text.
    // HOW: Apply an update containing both partial and final events, then confirm that the
    // assembled transcript contains only the final segments in order.
    // WHY: Partial events are transient UI/runtime hints, while clipboard output must be
    // built only from finalized transcript text.
    TranscriptAssembler assembler;
    assembler.applyUpdate(TranscriptUpdate{
        .events = {
            TranscriptEvent{.kind = TranscriptEventKind::Partial, .text = QStringLiteral("partial")},
            TranscriptEvent{.kind = TranscriptEventKind::Final, .text = QStringLiteral("hello")},
            TranscriptEvent{.kind = TranscriptEventKind::Final, .text = QStringLiteral("world")},
        },
        .error = {},
    });

    QCOMPARE(assembler.finalTranscript(), QStringLiteral("hello world"));
}

void StreamingTranscriptionTest::transcriptAssemblerResetClearsFinalAndPartialState()
{
    // WHAT: Verify that resetting the transcript assembler clears both final transcript and
    // transient partial state before new updates are applied.
    // HOW: Apply partial and final events, reset the assembler, then feed a new final event
    // and confirm that the old transcript content does not leak into the new output.
    // WHY: Session reuse and retry flows depend on clean transcript state so one utterance
    // cannot accidentally contaminate the next assembled result.
    TranscriptAssembler assembler;
    assembler.applyUpdate(TranscriptUpdate{
        .events = {
            TranscriptEvent{.kind = TranscriptEventKind::Partial, .text = QStringLiteral("stale partial")},
            TranscriptEvent{.kind = TranscriptEventKind::Final, .text = QStringLiteral("stale final")},
        },
        .error = {},
    });

    assembler.reset();
    assembler.applyUpdate(TranscriptUpdate{
        .events = {
            TranscriptEvent{.kind = TranscriptEventKind::Partial, .text = QStringLiteral("new partial")},
            TranscriptEvent{.kind = TranscriptEventKind::Final, .text = QStringLiteral("fresh")},
        },
        .error = {},
    });

    QCOMPARE(assembler.finalTranscript(), QStringLiteral("fresh"));
}

void StreamingTranscriptionTest::compatPathStreamsChunksAndAssemblesFinalTranscript()
{
    // WHAT: Verify that the compatibility transcription path normalizes, chunks, streams,
    // and assembles a final transcript through the session interface.
    // HOW: Normalize a small stereo recording, run it through a fake streaming session, and
    // confirm the final text plus the chunking side effects observed by the session.
    // WHY: The daemon and one-shot flows still depend on this wrapper, so it must preserve
    // the streaming-first runtime contract while returning a final transcript.
    const Recording recording = makeStereoRecording();
    const RecordingNormalizer normalizer;
    FakeStreamingSession session;

    const TranscriptionResult result = transcribeRecordingViaStreaming(session, recording, normalizer);

    QVERIFY(result.success);
    QCOMPARE(result.text, QStringLiteral("hello world"));
    QVERIFY(session.finishCalled());
    QCOMPARE(session.chunkCount(), 1);
    QVERIFY(session.sampleCount() > 0);
    QCOMPARE(session.frameOffsets().at(0), 0);
}

void StreamingTranscriptionTest::compatPathReportsNormalizationFailure()
{
    // WHAT: Verify that the compatibility path reports normalization failures as structured
    // runtime errors before any backend session work starts.
    // HOW: Pass an empty recording through the wrapper and check that it fails with the
    // audio-normalization error category without invoking session finish.
    // WHY: Clear failure reporting at the runtime seam keeps recorder/input issues distinct
    // from backend decode failures and avoids corrupt session state.
    const Recording recording;
    const RecordingNormalizer normalizer;
    FakeStreamingSession session;

    const TranscriptionResult result = transcribeRecordingViaStreaming(session, recording, normalizer);

    QVERIFY(!result.success);
    QCOMPARE(result.error.code, RuntimeErrorCode::AudioNormalizationFailed);
    QVERIFY(!session.finishCalled());
    QCOMPARE(session.chunkCount(), 0);
}

void StreamingTranscriptionTest::compatPathReturnsChunkPushFailure()
{
    // WHAT: Verify that a chunk-level backend failure is returned immediately by the
    // compatibility wrapper.
    // HOW: Use a fake session whose first pushed chunk returns a decode failure, then check
    // that the wrapper returns that error without calling finish.
    // WHY: Streaming decode failures should surface at the exact failing step so the caller
    // can react without trusting a partial finalization path.
    class FailingChunkSession final : public FakeStreamingSession
    {
    public:
        [[nodiscard]] TranscriptUpdate pushAudioChunk(const AudioChunk &chunk) override
        {
            Q_UNUSED(chunk);
            return TranscriptUpdate{
                .events = {},
                .error = RuntimeError{
                    .code = RuntimeErrorCode::DecodeFailed,
                    .message = QStringLiteral("chunk failed"),
                },
            };
        }
    };

    const Recording recording = makeStereoRecording();
    const RecordingNormalizer normalizer;
    FailingChunkSession session;

    const TranscriptionResult result = transcribeRecordingViaStreaming(session, recording, normalizer);

    QVERIFY(!result.success);
    QCOMPARE(result.error.code, RuntimeErrorCode::DecodeFailed);
    QCOMPARE(result.error.message, QStringLiteral("chunk failed"));
    QVERIFY(!session.finishCalled());
}

void StreamingTranscriptionTest::compatPathReturnsFinishFailure()
{
    // WHAT: Verify that a finalization failure from the backend session is returned by the
    // compatibility wrapper after chunk streaming succeeds.
    // HOW: Stream a valid recording through a fake session whose finish step returns a
    // decode failure, then confirm the wrapper returns that terminal error.
    // WHY: Finish-time decode errors are part of the live runtime contract, so callers must
    // see them as structured failures rather than as empty transcripts.
    class FailingFinishSession final : public FakeStreamingSession
    {
    public:
        [[nodiscard]] TranscriptUpdate finish() override
        {
            return TranscriptUpdate{
                .events = {},
                .error = RuntimeError{
                    .code = RuntimeErrorCode::DecodeFailed,
                    .message = QStringLiteral("finish failed"),
                },
            };
        }
    };

    const Recording recording = makeStereoRecording();
    const RecordingNormalizer normalizer;
    FailingFinishSession session;

    const TranscriptionResult result = transcribeRecordingViaStreaming(session, recording, normalizer);

    QVERIFY(!result.success);
    QCOMPARE(result.error.code, RuntimeErrorCode::DecodeFailed);
    QCOMPARE(result.error.message, QStringLiteral("finish failed"));
    QCOMPARE(session.chunkCount(), 1);
}

void StreamingTranscriptionTest::cancelReturnsStructuredFailure()
{
    // WHAT: Verify that session cancellation returns a structured cancelled error.
    // HOW: Call the fake session's cancel path directly and inspect the returned error and
    // the internal cancellation marker.
    // WHY: Cancellation is a distinct runtime outcome that worker and UI code may handle
    // differently from decode or configuration failures.
    FakeStreamingSession session;
    const TranscriptUpdate update = session.cancel();

    QVERIFY(!update.isOk());
    QCOMPARE(update.error.code, RuntimeErrorCode::Cancelled);
    QVERIFY(session.cancelCalled());
}

QTEST_APPLESS_MAIN(StreamingTranscriptionTest)

#include "streamingtranscriptiontest.moc"
