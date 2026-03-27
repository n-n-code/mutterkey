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
    void normalizeForRuntimeDownmixesStereoInput();
    void normalizeForRuntimeResamplesMonoInput();
    void normalizeForRuntimeAcceptsAlreadyNormalizedMonoInput();
    void normalizeForRuntimeRejectsEmptyRecording();
    void normalizeForRuntimeRejectsInvalidSampleFormat();
    void normalizeForRuntimeRejectsIncompleteFrames();
    void normalizeForRuntimeTruncatesTailBytesAfterCompleteFrames();
};

} // namespace

void RecordingNormalizerTest::normalizeForRuntimeDownmixesStereoInput()
{
    // WHAT: Verify that stereo PCM input is downmixed to Whisper's mono format.
    // HOW: Feed the normalizer a short stereo recording with matching left/right samples
    // and check that the output becomes mono float samples with the expected amplitudes.
    // WHY: Mutterkey may capture audio in more than one channel, but Whisper expects mono
    // input, so the conversion step must preserve meaning while changing format.
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    const Recording recording = makeRecording(format, {16384, 16384, -16384, -16384});

    const RecordingNormalizer normalizer;
    NormalizedAudio normalizedAudio;
    QString errorMessage;
    const bool ok = normalizer.normalizeForRuntime(recording, &normalizedAudio, &errorMessage);

    QVERIFY2(ok, qPrintable(errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(normalizedAudio.sampleRate, 16000);
    QCOMPARE(normalizedAudio.channels, 1);
    QCOMPARE(normalizedAudio.samples.size(), size_t{2});
    QVERIFY(std::abs(normalizedAudio.samples.at(0) - 0.5f) < 0.0001f);
    QVERIFY(std::abs(normalizedAudio.samples.at(1) + 0.5f) < 0.0001f);
}

void RecordingNormalizerTest::normalizeForRuntimeResamplesMonoInput()
{
    // WHAT: Verify that mono input is resampled to Whisper's required 16 kHz rate.
    // HOW: Provide an 8 kHz recording, run normalization, and check that the output sample
    // rate and interpolated sample values match the expected 16 kHz result.
    // WHY: Recordings can arrive at device-native rates, and transcription quality depends
    // on the model receiving audio in the format it expects.
    QAudioFormat format;
    format.setSampleRate(8000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    const Recording recording = makeRecording(format, {0, 16384});

    const RecordingNormalizer normalizer;
    NormalizedAudio normalizedAudio;
    QString errorMessage;
    const bool ok = normalizer.normalizeForRuntime(recording, &normalizedAudio, &errorMessage);

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

void RecordingNormalizerTest::normalizeForRuntimeAcceptsAlreadyNormalizedMonoInput()
{
    // WHAT: Verify that already compatible mono 16 kHz PCM is accepted as-is.
    // HOW: Normalize an input that already matches Whisper's required structure and confirm
    // that the output stays mono, 16 kHz, and correctly scaled into float sample values.
    // WHY: The normalizer should not damage good input, because this is the fast path for
    // devices that already capture in the preferred format.
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    const Recording recording = makeRecording(format, {-32768, 0, 32767});

    const RecordingNormalizer normalizer;
    NormalizedAudio normalizedAudio;
    QString errorMessage;
    const bool ok = normalizer.normalizeForRuntime(recording, &normalizedAudio, &errorMessage);

    QVERIFY2(ok, qPrintable(errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(normalizedAudio.sampleRate, 16000);
    QCOMPARE(normalizedAudio.channels, 1);
    QCOMPARE(normalizedAudio.samples.size(), size_t{3});
    QVERIFY(std::abs(normalizedAudio.samples.at(0) + 1.0f) < 0.0001f);
    QVERIFY(std::abs(normalizedAudio.samples.at(1) - 0.0f) < 0.0001f);
    QVERIFY(std::abs(normalizedAudio.samples.at(2) - 0.9999695f) < 0.0001f);
}

void RecordingNormalizerTest::normalizeForRuntimeRejectsEmptyRecording()
{
    // WHAT: Verify that an empty recording is rejected before any normalization work starts.
    // HOW: Pass a default-constructed recording and confirm that normalization fails with an
    // explicit "Recording is empty" error.
    // WHY: Empty capture data is a common early failure mode, and rejecting it clearly keeps
    // later audio-format errors from hiding the real problem.
    const RecordingNormalizer normalizer;
    NormalizedAudio normalizedAudio;
    QString errorMessage;
    const bool ok = normalizer.normalizeForRuntime(Recording{}, &normalizedAudio, &errorMessage);

    QVERIFY(!ok);
    QCOMPARE(errorMessage, QStringLiteral("Recording is empty"));
    QVERIFY(!normalizedAudio.isValid());
}

void RecordingNormalizerTest::normalizeForRuntimeRejectsInvalidSampleFormat()
{
    // WHAT: Verify that unsupported sample formats are rejected.
    // HOW: Pass in a recording that uses floating-point samples instead of 16-bit PCM and
    // confirm that normalization fails with the expected error.
    // WHY: Whisper integration currently supports a specific capture format, and rejecting
    // incompatible data early avoids undefined audio conversion behavior.
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
    const bool ok = normalizer.normalizeForRuntime(recording, &normalizedAudio, &errorMessage);

    QVERIFY(!ok);
    QCOMPARE(errorMessage, QStringLiteral("Embedded Whisper only supports 16-bit PCM capture"));
    QVERIFY(!normalizedAudio.isValid());
}

void RecordingNormalizerTest::normalizeForRuntimeRejectsIncompleteFrames()
{
    // WHAT: Verify that truncated PCM frame data is rejected.
    // HOW: Provide a byte buffer whose size does not contain a whole stereo frame and check
    // that normalization fails with an incomplete-frame error.
    // WHY: Partial frame data usually means corrupted capture or transport data, and using
    // it would make downstream audio interpretation unreliable.
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
    const bool ok = normalizer.normalizeForRuntime(recording, &normalizedAudio, &errorMessage);

    QVERIFY(!ok);
    QCOMPARE(errorMessage, QStringLiteral("Recording does not contain complete PCM frames"));
    QVERIFY(!normalizedAudio.isValid());
}

void RecordingNormalizerTest::normalizeForRuntimeTruncatesTailBytesAfterCompleteFrames()
{
    // WHAT: Verify that extra tail bytes after a complete frame are ignored rather than breaking normalization.
    // HOW: Build stereo PCM data containing one full frame plus one extra byte, normalize it,
    // and confirm that the result contains exactly one valid downmixed sample.
    // WHY: Real byte streams can end with leftover bytes, and the normalizer is expected to
    // salvage the valid frames instead of failing once it has enough complete audio data.
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    Recording recording;
    recording.format = format;
    appendSample(&recording.pcmData, 16384);
    appendSample(&recording.pcmData, -16384);
    recording.pcmData.append('\0');
    recording.durationSeconds = 0.25;

    const RecordingNormalizer normalizer;
    NormalizedAudio normalizedAudio;
    QString errorMessage;
    const bool ok = normalizer.normalizeForRuntime(recording, &normalizedAudio, &errorMessage);

    QVERIFY2(ok, qPrintable(errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(normalizedAudio.sampleRate, 16000);
    QCOMPARE(normalizedAudio.channels, 1);
    QCOMPARE(normalizedAudio.samples.size(), size_t{1});
    QVERIFY(std::abs(normalizedAudio.samples.at(0) - 0.0f) < 0.0001f);
}

QTEST_APPLESS_MAIN(RecordingNormalizerTest)

#include "recordingnormalizertest.moc"
