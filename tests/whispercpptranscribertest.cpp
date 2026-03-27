#include "transcription/transcriptionengine.h"
#include "transcription/whispercpptranscriber.h"

#include <QtTest/QTest>

namespace {

class WhisperCppTranscriberTest final : public QObject
{
    Q_OBJECT

private slots:
    void whisperEngineSurfacesMissingModelAtLoadTime();
    void whisperRuntimeRejectsUnsupportedLanguage();
};

} // namespace

void WhisperCppTranscriberTest::whisperEngineSurfacesMissingModelAtLoadTime()
{
    TranscriberConfig config;
    config.modelPath = QStringLiteral("/tmp/definitely-missing-mutterkey-model.bin");

    const std::shared_ptr<const TranscriptionEngine> engine = createTranscriptionEngine(config);
    RuntimeError error;
    const std::shared_ptr<const TranscriptionModelHandle> model = engine->loadModel(&error);

    QVERIFY(model == nullptr);
    QCOMPARE(error.code, RuntimeErrorCode::ModelNotFound);
    QVERIFY(error.message.contains(QStringLiteral("Embedded Whisper model not found")));
}

void WhisperCppTranscriberTest::whisperRuntimeRejectsUnsupportedLanguage()
{
    TranscriberConfig config;
    config.modelPath = QStringLiteral("/tmp/unused.bin");
    config.language = QStringLiteral("pirate");
    WhisperCppTranscriber transcriber(config, std::shared_ptr<const TranscriptionModelHandle>{});

    const TranscriptUpdate update = transcriber.finish();

    QVERIFY(!update.isOk());
    QCOMPARE(update.error.code, RuntimeErrorCode::UnsupportedLanguage);
    QVERIFY(update.error.message.contains(QStringLiteral("pirate")));
}

QTEST_APPLESS_MAIN(WhisperCppTranscriberTest)

#include "whispercpptranscribertest.moc"
