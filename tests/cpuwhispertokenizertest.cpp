#include "asr/nativecpu/cpuwhispertokenizer.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QTest>

namespace {

class CpuWhisperTokenizerTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsPackagedTokenizerAssets();
    void tokenizesWhitespaceSeparatedTranscriptWithMerges();
};

bool writeTextFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

} // namespace

void CpuWhisperTokenizerTest::loadsPackagedTokenizerAssets()
{
    // WHAT: Verify that the packaged Whisper-family tokenizer loader reads both vocab and merge assets.
    // HOW: Write small temporary vocab and merge files, then load them through the app-owned tokenizer helper.
    // WHY: The native decoder path needs its tokenizer contract to be package-owned rather than implicit in whisper.cpp.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString vocabularyPath = tempDir.filePath(QStringLiteral("tokenizer.txt"));
    const QString mergesPath = tempDir.filePath(QStringLiteral("tokenizer.merges"));
    QVERIFY(writeTextFile(vocabularyPath, "h\ne\nhe\nhello\nw\nworld\n"));
    QVERIFY(writeTextFile(mergesPath, "h e\nhe l\nhel l\nhell o\n"));

    RuntimeError error;
    const std::optional<CpuWhisperTokenizerModel> model =
        loadCpuWhisperTokenizerModel(vocabularyPath, mergesPath, &error);

    if (!model.has_value()) {
        QFAIL("Expected tokenizer model");
        return;
    }
    QVERIFY(error.isOk());
    const CpuWhisperTokenizerModel &tokenizerModel = model.value();
    QCOMPARE(tokenizerModel.vocabulary.size(), static_cast<std::size_t>(6));
    QCOMPARE(tokenizerModel.merges.size(), static_cast<std::size_t>(4));
}

void CpuWhisperTokenizerTest::tokenizesWhitespaceSeparatedTranscriptWithMerges()
{
    // WHAT: Verify that the packaged tokenizer emits merged tokens for a simple transcript.
    // HOW: Load a temporary tokenizer whose merge rules can collapse `hello`, then tokenize `hello world`.
    // WHY: Phase 5B needs a real packaged tokenizer seam so token ids no longer depend on whitespace splitting.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString vocabularyPath = tempDir.filePath(QStringLiteral("tokenizer.txt"));
    const QString mergesPath = tempDir.filePath(QStringLiteral("tokenizer.merges"));
    QVERIFY(writeTextFile(vocabularyPath, "h\ne\nl\no\nhello\nw\nwo\nr\nl\nd\n"));
    QVERIFY(writeTextFile(mergesPath, "h e\nhe l\nhel l\nhell o\nw o\nwo r\nwor l\nworl d\n"));

    RuntimeError error;
    const std::optional<CpuWhisperTokenizerModel> model =
        loadCpuWhisperTokenizerModel(vocabularyPath, mergesPath, &error);

    if (!model.has_value()) {
        QFAIL("Expected tokenizer model");
        return;
    }
    QVERIFY(error.isOk());
    const CpuWhisperTokenizerModel &tokenizerModel = model.value();
    const std::vector<CpuDecodedToken> tokens = tokenizeCpuTranscriptWhisper(QStringLiteral("hello world"), tokenizerModel);
    QVERIFY(!tokens.empty());
    QCOMPARE(tokens.front().text, QStringLiteral("hello"));
}

QTEST_APPLESS_MAIN(CpuWhisperTokenizerTest)

#include "cpuwhispertokenizertest.moc"
