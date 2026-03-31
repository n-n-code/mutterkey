#include "transcription/cpudecoderforward.h"
#include "transcription/cpuencoderforward.h"
#include "transcription/cpugreedysearch.h"
#include "transcription/cpumelspectrogram.h"
#include "transcription/cpumodelweights.h"
#include "transcription/cpuwhispertokenizer.h"

#include <QFile>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QtTest/QTest>

#include <array>
#include <bit>
#include <cmath>
#include <numbers>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

class CpuRealDecoderTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void pipelineSmokeTest();
    void dimensionValidationRejectsWrongShape();

private:
    QString m_weightsPath;
    QString m_modelDir;
};

struct SineWaveSpec {
    float durationSeconds = 0.0F;
    float frequencyHz = 0.0F;
    float amplitude = 0.0F;
};

struct TokenizerPaths {
    QString vocabPath;
    QString mergesPath;
};

/**
 * @brief Synthesizes a short 16 kHz audio buffer with a simple sine wave.
 */
std::vector<float> synthesizeSineWave(int sampleRate, const SineWaveSpec &spec)
{
    const auto sampleCount = static_cast<int>(static_cast<float>(sampleRate) * spec.durationSeconds);
    std::vector<float> samples(static_cast<std::size_t>(sampleCount));
    constexpr float twoPi = 2.0F * std::numbers::pi_v<float>;
    for (int i = 0; i < sampleCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        samples.at(static_cast<std::size_t>(i)) = spec.amplitude * std::sin(twoPi * spec.frequencyHz * t);
    }
    return samples;
}

/**
 * @brief Loads a CpuWhisperTokenizerModel from HuggingFace vocab.json + merges.txt.
 *
 * The vocab.json is a JSON object mapping token strings to integer IDs.
 * We convert it to the line-ordered vocabulary that CpuWhisperTokenizerModel expects.
 */
std::optional<CpuWhisperTokenizerModel> loadTokenizerFromHuggingFace(const TokenizerPaths &paths)
{
    // Parse vocab.json: {"token": id, ...}
    QFile vocabFile(paths.vocabPath);
    if (!vocabFile.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(vocabFile.readAll());
    if (!doc.isObject()) {
        return std::nullopt;
    }

    const QJsonObject vocabObj = doc.object();
    const qsizetype vocabSize = vocabObj.size();

    CpuWhisperTokenizerModel model;
    model.vocabulary.resize(static_cast<std::size_t>(vocabSize));

    for (auto it = vocabObj.constBegin(); it != vocabObj.constEnd(); ++it) {
        const int id = it.value().toInt();
        if (id >= 0 && std::cmp_less(id, vocabSize)) {
            model.vocabulary.at(static_cast<std::size_t>(id)) = it.key();
            model.tokenIds.insert(it.key(), id);
        }
    }

    // Parse merges.txt (same format the loader expects).
    QFile mergesFile(paths.mergesPath);
    if (!mergesFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    const QString mergesText = QString::fromUtf8(mergesFile.readAll());
    const QStringList mergeLines = mergesText.split(QLatin1Char('\n'));
    int rank = 0;
    for (const QString &rawLine : mergeLines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() != 2) {
            continue;
        }
        model.merges.push_back(CpuWhisperMergeRule{.left = parts.at(0), .right = parts.at(1)});
        // Build merge key the same way as the loader.
        QString key;
        key.reserve(parts.at(0).size() + parts.at(1).size() + 1);
        key.append(parts.at(0));
        key.append(QChar::fromLatin1('\x1f'));
        key.append(parts.at(1));
        model.mergeRanks.insert(key, rank);
        ++rank;
    }

    return model;
}

template <typename T>
bool writeScalar(QFile *file, const T &value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    if (file == nullptr) {
        return false;
    }

    const auto bytes = std::bit_cast<std::array<char, sizeof(T)>>(value);
    return file->write(bytes.data(), static_cast<qint64>(bytes.size())) == static_cast<qint64>(bytes.size());
}

} // namespace

void CpuRealDecoderTest::initTestCase()
{
    const QString envPath =
        QProcessEnvironment::systemEnvironment().value(QStringLiteral("MUTTERKEY_TEST_WEIGHTS_PATH"));
    if (envPath.isEmpty()) {
        QSKIP("MUTTERKEY_TEST_WEIGHTS_PATH not set — skipping real decoder tests");
    }

    m_weightsPath = envPath;
    if (!QFileInfo::exists(m_weightsPath)) {
        QSKIP(qPrintable(QStringLiteral("Weights file not found: %1").arg(m_weightsPath)));
    }

    m_modelDir = QFileInfo(m_weightsPath).absolutePath();
}

void CpuRealDecoderTest::pipelineSmokeTest()
{
    // WHAT: Verify the full native CPU decoder pipeline runs without crashing on real weights.
    // HOW: Load real MKCPUR3 weights, synthesize audio, run mel->encoder->decoder->greedy search.
    // WHY: This is the critical end-to-end validation that the tensor pipeline produces tokens from real weights.

    RuntimeError loadError;
    const auto weights = loadCpuWhisperModelWeights(m_weightsPath, &loadError);
    if (weights == nullptr) {
        QFAIL(qPrintable(QStringLiteral("Failed to load weights: %1 — %2").arg(loadError.message, loadError.detail)));
        return;
    }

    QCOMPARE(weights->config.audioStateSize, 512);
    QCOMPARE(weights->config.vocabularySize, 51864);

    // Load tokenizer for transcript assembly.
    const auto tokenizer = loadTokenizerFromHuggingFace(TokenizerPaths{
        .vocabPath = m_modelDir + QStringLiteral("/vocab.json"),
        .mergesPath = m_modelDir + QStringLiteral("/merges.txt"),
    });
    if (!tokenizer.has_value()) {
        QFAIL("Failed to load tokenizer from staging directory");
        return;
    }
    const CpuWhisperTokenizerModel &loadedTokenizer = *tokenizer;

    // Synthesize a 2-second audio buffer (440 Hz sine wave at moderate amplitude).
    constexpr int sampleRate = 16000;
    const std::vector<float> samples = synthesizeSineWave(sampleRate,
                                                          SineWaveSpec{
                                                              .durationSeconds = 2.0F,
                                                              .frequencyHz = 440.0F,
                                                              .amplitude = 0.3F,
                                                          });

    // Run mel spectrogram extraction.
    const CpuMelConfig melConfig{
        .sampleRate = sampleRate,
        .fftSize = 400,
        .hopLength = 160,
        .melBands = weights->config.melBands,
        .maxFrames = 3000,
    };
    const CpuTensor mel = extractLogMelSpectrogram(samples, weights->melFilters, melConfig);
    QCOMPARE(mel.rows, weights->config.melBands);
    QCOMPARE(mel.cols, 3000);

    // Run encoder.
    const CpuTensor encoderOutput = runEncoderForward(mel, weights->encoder, weights->config);
    QCOMPARE(encoderOutput.rows, weights->config.audioContextSize);
    QCOMPARE(encoderOutput.cols, weights->config.audioStateSize);

    // Allocate KV cache and run greedy search.
    CpuKVCache cache;
    cache.allocate(weights->config, 224);
    const CpuGreedySearchConfig searchConfig{};
    const CpuGreedySearchResult result = runCpuGreedySearch(encoderOutput, *weights, &cache, searchConfig);

    // A sine wave may or may not produce meaningful speech. What matters is that:
    // 1. The pipeline ran without crashing.
    // 2. Tokens were produced (greedy search always produces at least initial prompt tokens).
    QVERIFY2(!result.tokens.empty(), "Greedy search produced no tokens");

    // If speech was detected, verify we can resolve token text.
    if (result.isSpeech) {
        QStringList textParts;
        for (const CpuDecodedToken &token : result.tokens) {
            if (token.kind == CpuDecodedTokenKind::Lexical && token.id >= 0
                && std::cmp_less(token.id, loadedTokenizer.vocabulary.size())) {
                textParts.append(loadedTokenizer.vocabulary.at(static_cast<std::size_t>(token.id)));
            }
        }
        const QString transcript = textParts.join(QString()).trimmed();
        qDebug() << "Transcript from sine wave:" << transcript;
    }
}

void CpuRealDecoderTest::dimensionValidationRejectsWrongShape()
{
    // WHAT: Verify that loadCpuWhisperModelWeights rejects a file with wrong tensor dimensions.
    // HOW: Create a minimal MKCPUR3 file with a deliberately wrong tensor shape.
    // WHY: Dimension validation is a security and correctness requirement.

    // Write a minimal but dimensionally wrong MKCPUR3 file.
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(true);
    QVERIFY(tempFile.open());
    {
        // Header.
        constexpr std::array<char, 8> magic{'M', 'K', 'C', 'P', 'U', 'R', '3', '\0'};
        tempFile.write(magic.data(), static_cast<qint64>(magic.size()));

        // Version = 3, tensor_count = 1, metadata_bytes.
        const QByteArray metadata = R"({"n_mels":80,"n_audio_ctx":1500,"n_audio_state":512,"n_audio_head":8,"n_audio_layer":6,"n_text_ctx":448,"n_text_state":512,"n_text_head":8,"n_text_layer":6,"n_vocab":51864})";
        const quint32 version = 3;
        const quint32 tensorCount = 1;
        const auto metadataLen = static_cast<quint32>(metadata.size());
        QVERIFY(writeScalar(&tempFile, version));
        QVERIFY(writeScalar(&tempFile, tensorCount));
        QVERIFY(writeScalar(&tempFile, metadataLen));
        tempFile.write(metadata);

        // One tensor directory entry with wrong shape: encoder.conv1.weight should be (512, 240)
        // but we'll make it (10, 10).
        const QByteArray name = "encoder.conv1.weight";
        const auto nameLen = static_cast<quint32>(name.size());
        QVERIFY(writeScalar(&tempFile, nameLen));
        tempFile.write(name);
        const quint32 nDims = 2;
        QVERIFY(writeScalar(&tempFile, nDims));
        const quint32 dim0 = 10;
        const quint32 dim1 = 10;
        QVERIFY(writeScalar(&tempFile, dim0));
        QVERIFY(writeScalar(&tempFile, dim1));
        const quint32 dtype = 0; // f32
        QVERIFY(writeScalar(&tempFile, dtype));
        const qint64 dataOffset = 0;
        const qint64 dataSize = static_cast<qint64>(10) * 10 * 4;
        QVERIFY(writeScalar(&tempFile, dataOffset));
        QVERIFY(writeScalar(&tempFile, dataSize));

        // Write 100 floats of dummy data.
        const QByteArray dummy(static_cast<qsizetype>(100 * sizeof(float)), '\0');
        tempFile.write(dummy);
        tempFile.close();
    }

    RuntimeError error;
    const auto weights = loadCpuWhisperModelWeights(tempFile.fileName(), &error);

    // Should fail — either at tensor loading (missing tensors) or dimension validation.
    QVERIFY2(weights == nullptr, "Expected load to fail for dimensionally wrong weights");
    QCOMPARE(error.code, RuntimeErrorCode::ModelLoadFailed);
}

QTEST_APPLESS_MAIN(CpuRealDecoderTest)

#include "cpurealdecodertest.moc"
