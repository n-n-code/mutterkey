#include "audio/audiorecorder.h"
#include "clipboardwriter.h"
#include "hotkeymanager.h"

#include <QAudioFormat>
#include <QtTest/QTest>

namespace {

class PlatformLogicTest final : public QObject
{
    Q_OBJECT

private slots:
    void clipboardBackendNamePrefersSystemClipboard();
    void clipboardBackendNameFallsBackToQtClipboard();
    void clipboardRoundTripRequiresExactTextMatch();
    void resolveRecordingFormatReturnsRequestedInt16FormatWhenSupported();
    void resolveRecordingFormatFallsBackToPreferredInt16Format();
    void resolveRecordingFormatRejectsNonInt16PreferredFormat();
    void parseConfiguredKeySequenceAcceptsPortableText();
    void parseConfiguredKeySequenceAcceptsSingleCharacterShortcut();
    void keySequenceHelpersProduceDiagnosticShapes();
};

} // namespace

void PlatformLogicTest::clipboardBackendNamePrefersSystemClipboard()
{
    // WHAT: Verify that clipboard backend diagnostics prefer KDE system clipboard support when
    // both clipboard paths are available.
    // HOW: Ask the helper for a backend name with both system and Qt clipboard flags enabled
    // and compare the returned diagnostic label.
    // WHY: Background-service clipboard behavior is KDE-first by design, so diagnostics
    // should report that primary backend consistently.
    QCOMPARE(clipboardBackendName(true, true), QStringLiteral("KSystemClipboard"));
}

void PlatformLogicTest::clipboardBackendNameFallsBackToQtClipboard()
{
    // WHAT: Verify that clipboard backend diagnostics fall back to Qt clipboard naming when
    // KDE system clipboard support is unavailable.
    // HOW: Ask the helper for backend names with only Qt clipboard support and with neither
    // backend available, then compare the returned labels.
    // WHY: Operators need clear diagnostics about which clipboard path is active or missing
    // when troubleshooting service-side copy behavior.
    QCOMPARE(clipboardBackendName(false, true), QStringLiteral("Qt QClipboard"));
    QCOMPARE(clipboardBackendName(false, false), QStringLiteral("unavailable"));
}

void PlatformLogicTest::clipboardRoundTripRequiresExactTextMatch()
{
    // WHAT: Verify that clipboard round-trip success requires an exact text match.
    // HOW: Compare one helper call with identical text and another with trimmed or altered
    // text to confirm the helper only accepts exact readback.
    // WHY: Clipboard writes are treated as verified only when the backend returns exactly the
    // requested content, not a normalized or partially matching variant.
    QVERIFY(clipboardRoundTripSucceeded(QStringLiteral("hello"), QStringLiteral("hello")));
    QVERIFY(!clipboardRoundTripSucceeded(QStringLiteral("hello"), QStringLiteral("hello ")));
}

void PlatformLogicTest::resolveRecordingFormatReturnsRequestedInt16FormatWhenSupported()
{
    // WHAT: Verify that recorder format resolution keeps the configured Int16 format when the
    // target device reports that exact format as supported.
    // HOW: Request a 16 kHz stereo config, mark the requested format as supported, and
    // compare the resolved format against the requested sample rate, channels, and sample type.
    // WHY: Capture should stay on the product-requested format when possible so downstream
    // normalization sees the expected device metadata.
    AudioConfig config;
    config.sampleRate = 16000;
    config.channels = 2;

    QAudioFormat preferredFormat;
    preferredFormat.setSampleRate(48000);
    preferredFormat.setChannelCount(1);
    preferredFormat.setSampleFormat(QAudioFormat::Int16);

    const QAudioFormat format = resolveRecordingFormatForConfig(config, preferredFormat, true);

    QVERIFY(format.isValid());
    QCOMPARE(format.sampleRate(), 16000);
    QCOMPARE(format.channelCount(), 2);
    QCOMPARE(format.sampleFormat(), QAudioFormat::Int16);
}

void PlatformLogicTest::resolveRecordingFormatFallsBackToPreferredInt16Format()
{
    // WHAT: Verify that recorder format resolution falls back to the device's preferred Int16
    // format when the requested capture format is unsupported.
    // HOW: Mark the requested format as unsupported, provide a valid Int16 preferred format,
    // and compare the resolved result against that preferred fallback.
    // WHY: This is the product's supported compatibility path for devices that cannot capture
    // with the exact requested sample rate or channel count.
    AudioConfig config;
    config.sampleRate = 16000;
    config.channels = 1;

    QAudioFormat preferredFormat;
    preferredFormat.setSampleRate(48000);
    preferredFormat.setChannelCount(2);
    preferredFormat.setSampleFormat(QAudioFormat::Int16);

    const QAudioFormat format = resolveRecordingFormatForConfig(config, preferredFormat, false);

    QVERIFY(format.isValid());
    QCOMPARE(format.sampleRate(), preferredFormat.sampleRate());
    QCOMPARE(format.channelCount(), preferredFormat.channelCount());
    QCOMPARE(format.sampleFormat(), QAudioFormat::Int16);
}

void PlatformLogicTest::resolveRecordingFormatRejectsNonInt16PreferredFormat()
{
    // WHAT: Verify that recorder format resolution rejects devices whose fallback format is
    // not 16-bit PCM.
    // HOW: Mark the requested format as unsupported, supply a non-Int16 preferred format,
    // and confirm that the helper returns an invalid format with a clear error message.
    // WHY: Mutterkey's runtime normalization currently depends on 16-bit PCM capture, so the
    // recorder must fail clearly when no compatible fallback exists.
    AudioConfig config;
    config.sampleRate = 16000;
    config.channels = 1;

    QAudioFormat preferredFormat;
    preferredFormat.setSampleRate(48000);
    preferredFormat.setChannelCount(2);
    preferredFormat.setSampleFormat(QAudioFormat::Float);

    QString errorMessage;
    const QAudioFormat format = resolveRecordingFormatForConfig(config, preferredFormat, false, &errorMessage);

    QVERIFY(!format.isValid());
    QVERIFY(errorMessage.contains(QStringLiteral("16-bit PCM")));
}

void PlatformLogicTest::parseConfiguredKeySequenceAcceptsPortableText()
{
    // WHAT: Verify that configured shortcut parsing accepts portable-text sequences.
    // HOW: Parse a representative portable-text shortcut and compare the resulting sequence's
    // portable-text rendering with the expected normalized value.
    // WHY: Config values should round-trip cleanly across locales and saved settings, so the
    // parser must prefer stable portable-text forms.
    const QKeySequence sequence = parseConfiguredKeySequence(QStringLiteral("Meta+F8"));

    QVERIFY(!sequence.isEmpty());
    QCOMPARE(sequence.toString(QKeySequence::PortableText), QStringLiteral("Meta+F8"));
}

void PlatformLogicTest::parseConfiguredKeySequenceAcceptsSingleCharacterShortcut()
{
    // WHAT: Verify that configured shortcut parsing accepts a single printable character as a
    // fallback shortcut form.
    // HOW: Parse a one-character shortcut and compare the normalized portable-text rendering
    // of the resulting sequence.
    // WHY: Simple one-key shortcuts are a supported ergonomic input style, so the parser must
    // not reject them just because no modifier is present.
    const QKeySequence sequence = parseConfiguredKeySequence(QStringLiteral("x"));

    QVERIFY(!sequence.isEmpty());
    QCOMPARE(sequence.toString(QKeySequence::PortableText), QStringLiteral("X"));
}

void PlatformLogicTest::keySequenceHelpersProduceDiagnosticShapes()
{
    // WHAT: Verify that key-sequence diagnostic helpers produce the expected JSON and text
    // shapes for assigned shortcuts.
    // HOW: Serialize a multi-part sequence into JSON, join a list of sequences into portable
    // text, and compare both outputs with the expected diagnostic values.
    // WHY: Diagnose output and shortcut-registration logs depend on these helper shapes when
    // explaining what KDE assigned to the action.
    const QKeySequence assignedSequence =
        QKeySequence::fromString(QStringLiteral("Ctrl+A, Ctrl+B"), QKeySequence::PortableText);
    const QJsonArray assignedJson = keySequenceToDiagnosticJson(assignedSequence);

    QCOMPARE(assignedJson.size(), 2);
    QCOMPARE(assignedJson.at(0).toString(), QStringLiteral("Ctrl+A"));
    QCOMPARE(assignedJson.at(1).toString(), QStringLiteral("Ctrl+B"));

    const QString assignedText =
        keySequenceListToPortableText({QKeySequence(QStringLiteral("Meta+F8")), QKeySequence()});
    QCOMPARE(assignedText, QStringLiteral("Meta+F8"));
}

QTEST_APPLESS_MAIN(PlatformLogicTest)

#include "platformlogicstest.moc"
