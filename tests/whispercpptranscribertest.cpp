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
    void legacyEngineDiagnosticsExplainFallback();
};

} // namespace

void WhisperCppTranscriberTest::whisperEngineSurfacesMissingModelAtLoadTime()
{
    // WHAT: Verify that the Whisper engine reports a missing model file at model-load time.
    // HOW: Create the real engine with a definitely missing model path, try to load the
    // model, and inspect the returned structured runtime error.
    // WHY: Model-path problems are a common setup failure, so the engine must reject them
    // before session creation with a precise, user-visible error category.
    TranscriberConfig config;
    config.modelPath = QStringLiteral("/tmp/definitely-missing-mutterkey-model.bin");

    const std::shared_ptr<const TranscriptionEngine> engine = createTranscriptionEngine(config);
    RuntimeError error;
    const std::shared_ptr<const TranscriptionModelHandle> model = engine->loadModel(&error);

    QVERIFY(model == nullptr);
    QCOMPARE(error.code, RuntimeErrorCode::ModelNotFound);
    QCOMPARE(error.code, RuntimeErrorCode::ModelNotFound);
}

void WhisperCppTranscriberTest::whisperRuntimeRejectsUnsupportedLanguage()
{
    // WHAT: Verify that the Whisper runtime rejects unsupported configured language values.
    // HOW: Construct the real transcriber with an unsupported language code and confirm that
    // finish returns the expected unsupported-language runtime error.
    // WHY: Backend-specific validation belongs at the runtime boundary, and callers need a
    // stable error instead of silent fallback when the language request is invalid.
    TranscriberConfig config;
    config.modelPath = QStringLiteral("/tmp/unused.bin");
    config.language = QStringLiteral("pirate");
    WhisperCppTranscriber transcriber(config, std::shared_ptr<const TranscriptionModelHandle>{});

    const TranscriptUpdate update = transcriber.finish();

    QVERIFY(!update.isOk());
    QCOMPARE(update.error.code, RuntimeErrorCode::UnsupportedLanguage);
    QVERIFY(update.error.message.contains(QStringLiteral("pirate")));
}

void WhisperCppTranscriberTest::legacyEngineDiagnosticsExplainFallback()
{
    // WHAT: Verify that the generic engine surfaces a legacy-selection reason when the model path cannot be inspected.
    // HOW: Construct the generic engine with a definitely missing model path and inspect the reported runtime diagnostics.
    // WHY: The runtime selector should explain why it fell back to the legacy backend so diagnose output can distinguish policy fallback from normal compatibility routing.
    TranscriberConfig config;
    config.modelPath = QStringLiteral("/tmp/definitely-missing-mutterkey-model.bin");

    const std::shared_ptr<const TranscriptionEngine> engine = createTranscriptionEngine(config);
    QVERIFY(engine != nullptr);
    QCOMPARE(engine->diagnostics().backendName, QStringLiteral("whisper.cpp"));
    QVERIFY(engine->diagnostics().selectionReason.contains(QStringLiteral("legacy whisper runtime"),
                                                          Qt::CaseInsensitive));
}

QTEST_APPLESS_MAIN(WhisperCppTranscriberTest)

#include "whispercpptranscribertest.moc"
