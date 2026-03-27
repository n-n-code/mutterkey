#include "audio/recording.h"
#include "audio/recordingnormalizer.h"
#include "transcription/audiochunker.h"
#include "transcription/transcriptassembler.h"
#include "transcription/transcriptioncompat.h"

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

class FakeStreamingSession final : public TranscriptionSession
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
    void transcriptAssemblerJoinsOnlyFinalEvents();
    void compatPathStreamsChunksAndAssemblesFinalTranscript();
    void cancelReturnsStructuredFailure();
};

} // namespace

void StreamingTranscriptionTest::audioChunkerSplitsNormalizedAudioIntoStableOffsets()
{
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

void StreamingTranscriptionTest::transcriptAssemblerJoinsOnlyFinalEvents()
{
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

void StreamingTranscriptionTest::compatPathStreamsChunksAndAssemblesFinalTranscript()
{
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

void StreamingTranscriptionTest::cancelReturnsStructuredFailure()
{
    FakeStreamingSession session;
    const TranscriptUpdate update = session.cancel();

    QVERIFY(!update.isOk());
    QCOMPARE(update.error.code, RuntimeErrorCode::Cancelled);
    QVERIFY(session.cancelCalled());
}

QTEST_APPLESS_MAIN(StreamingTranscriptionTest)

#include "streamingtranscriptiontest.moc"
