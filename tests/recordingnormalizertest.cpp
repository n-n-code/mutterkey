#include "audio/recordingnormalizer.h"

#include <cmath>
#include <QtTest/QTest>

namespace {

void appendSample(QByteArray *pcmData, qint16 sample)
{
    const auto sampleBits = static_cast<quint16>(sample);
    pcmData->append(static_cast<char>(static_cast<quint32>(sampleBits) & 0x00ffU));
    pcmData->append(static_cast<char>((static_cast<quint32>(sampleBits) >> 8U) & 0x00ffU));
}

Recording makeRecording(const QAudioFormat &format, std::initializer_list<qint16> pcmSamples)
{
    Recording recording;
    recording.format = format;

    for (const qint16 sample : pcmSamples) {
        appendSample(&recording.pcmData, sample);
    }

    const auto bytesPerFrame =
        static_cast<qsizetype>(sizeof(qint16)) * static_cast<qsizetype>(recording.format.channelCount());
    const auto frameCount = recording.pcmData.size() / bytesPerFrame;
    recording.durationSeconds =
        static_cast<double>(frameCount) / static_cast<double>(recording.format.sampleRate());
    return recording;
}

class RecordingNormalizerTest final : public QObject
{
    Q_OBJECT

private slots:
    void normalizeForWhisperDownmixesStereoInput();
    void normalizeForWhisperResamplesMonoInput();
    void normalizeForWhisperAcceptsAlreadyNormalizedMonoInput();
    void normalizeForWhisperRejectsInvalidSampleFormat();
    void normalizeForWhisperRejectsIncompleteFrames();
};

} // namespace

void RecordingNormalizerTest::normalizeForWhisperDownmixesStereoInput()
{
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    const Recording recording = makeRecording(format, {16384, 16384, -16384, -16384});

    const RecordingNormalizer normalizer;
    NormalizedAudio normalizedAudio;
    QString errorMessage;
    const bool ok = normalizer.normalizeForWhisper(recording, &normalizedAudio, &errorMessage);

    QVERIFY2(ok, qPrintable(errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(normalizedAudio.sampleRate, 16000);
    QCOMPARE(normalizedAudio.channels, 1);
    QCOMPARE(normalizedAudio.samples.size(), size_t{2});
    QVERIFY(std::abs(normalizedAudio.samples.at(0) - 0.5f) < 0.0001f);
    QVERIFY(std::abs(normalizedAudio.samples.at(1) + 0.5f) < 0.0001f);
}

void RecordingNormalizerTest::normalizeForWhisperResamplesMonoInput()
{
    QAudioFormat format;
    format.setSampleRate(8000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    const Recording recording = makeRecording(format, {0, 16384});

    const RecordingNormalizer normalizer;
    NormalizedAudio normalizedAudio;
    QString errorMessage;
    const bool ok = normalizer.normalizeForWhisper(recording, &normalizedAudio, &errorMessage);

    QVERIFY2(ok, qPrintable(errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(normalizedAudio.sampleRate, 16000);
    QCOMPARE(normalizedAudio.channels, 1);
    QCOMPARE(normalizedAudio.samples.size(), size_t{4});
    QVERIFY(std::abs(normalizedAudio.samples.at(0) - 0.0f) < 0.0001f);
    QVERIFY(std::abs(normalizedAudio.samples.at(1) - 0.25f) < 0.0001f);
    QVERIFY(std::abs(normalizedAudio.samples.at(2) - 0.5f) < 0.0001f);
    QVERIFY(std::abs(normalizedAudio.samples.at(3) - 0.5f) < 0.0001f);
}

void RecordingNormalizerTest::normalizeForWhisperAcceptsAlreadyNormalizedMonoInput()
{
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    const Recording recording = makeRecording(format, {-32768, 0, 32767});

    const RecordingNormalizer normalizer;
    NormalizedAudio normalizedAudio;
    QString errorMessage;
    const bool ok = normalizer.normalizeForWhisper(recording, &normalizedAudio, &errorMessage);

    QVERIFY2(ok, qPrintable(errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(normalizedAudio.sampleRate, 16000);
    QCOMPARE(normalizedAudio.channels, 1);
    QCOMPARE(normalizedAudio.samples.size(), size_t{3});
    QVERIFY(std::abs(normalizedAudio.samples.at(0) + 1.0f) < 0.0001f);
    QVERIFY(std::abs(normalizedAudio.samples.at(1) - 0.0f) < 0.0001f);
    QVERIFY(std::abs(normalizedAudio.samples.at(2) - 0.9999695f) < 0.0001f);
}

void RecordingNormalizerTest::normalizeForWhisperRejectsInvalidSampleFormat()
{
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Float);

    Recording recording;
    recording.format = format;
    recording.pcmData = QByteArray(4, '\0');
    recording.durationSeconds = 0.25;

    const RecordingNormalizer normalizer;
    NormalizedAudio normalizedAudio;
    QString errorMessage;
    const bool ok = normalizer.normalizeForWhisper(recording, &normalizedAudio, &errorMessage);

    QVERIFY(!ok);
    QCOMPARE(errorMessage, QStringLiteral("Embedded Whisper only supports 16-bit PCM capture"));
    QVERIFY(!normalizedAudio.isValid());
}

void RecordingNormalizerTest::normalizeForWhisperRejectsIncompleteFrames()
{
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    Recording recording;
    recording.format = format;
    recording.pcmData = QByteArray(3, '\0');
    recording.durationSeconds = 0.25;

    const RecordingNormalizer normalizer;
    NormalizedAudio normalizedAudio;
    QString errorMessage;
    const bool ok = normalizer.normalizeForWhisper(recording, &normalizedAudio, &errorMessage);

    QVERIFY(!ok);
    QCOMPARE(errorMessage, QStringLiteral("Recording does not contain complete PCM frames"));
    QVERIFY(!normalizedAudio.isValid());
}

QTEST_APPLESS_MAIN(RecordingNormalizerTest)

#include "recordingnormalizertest.moc"
