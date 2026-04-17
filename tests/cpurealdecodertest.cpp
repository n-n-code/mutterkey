#include "asr/model/modelpackage.h"
#include "asr/model/modelvalidator.h"
#include "asr/nativecpu/cpudecoderforward.h"
#include "asr/nativecpu/cpudecoderruntime.h"
#include "asr/nativecpu/cpumodelweights.h"
#include "asr/nativecpu/cpureferencemodel.h"
#include "asr/nativecpu/cpuwhispertokenizer.h"
#include "testutil/pcmwavreader.h"

#include <QFile>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QtTest/QTest>

#include <array>
#include <bit>
#include <initializer_list>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

class CpuRealDecoderTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void syntheticV3WeightsLoadWithDimensionValidation();
    void pipelineSmokeTest();
    void pipelineTranscribesJfkSample();
    void pipelineLoadsStagedPackage();
    void dimensionValidationRejectsWrongShape();

private:
    QString m_weightsPath;
    QString m_modelDir;
    QString m_packagePath;
};

struct TokenizerPaths {
    QString vocabPath;
    QString mergesPath;
};

struct SyntheticWeightConfig {
    int melBands = 2;
    int audioContextSize = 3;
    int audioStateSize = 2;
    int audioHeadCount = 1;
    int audioLayerCount = 1;
    int textContextSize = 4;
    int textStateSize = 2;
    int textHeadCount = 1;
    int textLayerCount = 1;
    int vocabularySize = 8;
    bool corruptConv1Shape = false;
};

struct TensorSpec {
    QByteArray name;
    std::vector<quint32> dims;
};

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

qint64 tensorDataBytes(const TensorSpec &tensor)
{
    qint64 elementCount = 1;
    for (const quint32 dim : tensor.dims) {
        elementCount *= static_cast<qint64>(dim);
    }
    return elementCount * static_cast<qint64>(sizeof(float));
}

QByteArray syntheticMetadataJson(const SyntheticWeightConfig &config)
{
    const QJsonObject object{
        {QStringLiteral("n_mels"), config.melBands},
        {QStringLiteral("n_audio_ctx"), config.audioContextSize},
        {QStringLiteral("n_audio_state"), config.audioStateSize},
        {QStringLiteral("n_audio_head"), config.audioHeadCount},
        {QStringLiteral("n_audio_layer"), config.audioLayerCount},
        {QStringLiteral("n_text_ctx"), config.textContextSize},
        {QStringLiteral("n_text_state"), config.textStateSize},
        {QStringLiteral("n_text_head"), config.textHeadCount},
        {QStringLiteral("n_text_layer"), config.textLayerCount},
        {QStringLiteral("n_vocab"), config.vocabularySize},
    };
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::vector<TensorSpec> syntheticTensorSpecs(const SyntheticWeightConfig &config)
{
    std::vector<TensorSpec> tensors;
    auto add = [&tensors](const char *name, std::initializer_list<int> dims) {
        std::vector<quint32> convertedDims;
        convertedDims.reserve(dims.size());
        for (const int dim : dims) {
            convertedDims.push_back(static_cast<quint32>(dim));
        }
        tensors.push_back(TensorSpec{.name = QByteArray(name), .dims = std::move(convertedDims)});
    };

    const int audioFfnSize = config.audioStateSize * 4;
    const int textFfnSize = config.textStateSize * 4;
    const int conv1Cols = (config.melBands * 3) + (config.corruptConv1Shape ? 1 : 0);

    add("encoder.conv1.weight", {config.audioStateSize, conv1Cols});
    add("encoder.conv1.bias", {1, config.audioStateSize});
    add("encoder.conv2.weight", {config.audioStateSize, config.audioStateSize * 3});
    add("encoder.conv2.bias", {1, config.audioStateSize});
    add("encoder.positional_embedding", {config.audioContextSize, config.audioStateSize});
    add("encoder.ln_post.weight", {1, config.audioStateSize});
    add("encoder.ln_post.bias", {1, config.audioStateSize});
    for (int layer = 0; layer < config.audioLayerCount; ++layer) {
        const QByteArray prefix = QByteArray("encoder.blocks.") + QByteArray::number(layer) + ".";
        add((prefix + "attn_ln.weight").constData(), {1, config.audioStateSize});
        add((prefix + "attn_ln.bias").constData(), {1, config.audioStateSize});
        add((prefix + "attn.query.weight").constData(), {config.audioStateSize, config.audioStateSize});
        add((prefix + "attn.query.bias").constData(), {1, config.audioStateSize});
        add((prefix + "attn.key.weight").constData(), {config.audioStateSize, config.audioStateSize});
        add((prefix + "attn.value.weight").constData(), {config.audioStateSize, config.audioStateSize});
        add((prefix + "attn.value.bias").constData(), {1, config.audioStateSize});
        add((prefix + "attn.out.weight").constData(), {config.audioStateSize, config.audioStateSize});
        add((prefix + "attn.out.bias").constData(), {1, config.audioStateSize});
        add((prefix + "mlp_ln.weight").constData(), {1, config.audioStateSize});
        add((prefix + "mlp_ln.bias").constData(), {1, config.audioStateSize});
        add((prefix + "mlp.0.weight").constData(), {audioFfnSize, config.audioStateSize});
        add((prefix + "mlp.0.bias").constData(), {1, audioFfnSize});
        add((prefix + "mlp.2.weight").constData(), {config.audioStateSize, audioFfnSize});
        add((prefix + "mlp.2.bias").constData(), {1, config.audioStateSize});
    }

    add("decoder.token_embedding.weight", {config.vocabularySize, config.textStateSize});
    add("decoder.positional_embedding", {config.textContextSize, config.textStateSize});
    add("decoder.ln.weight", {1, config.textStateSize});
    add("decoder.ln.bias", {1, config.textStateSize});
    for (int layer = 0; layer < config.textLayerCount; ++layer) {
        const QByteArray prefix = QByteArray("decoder.blocks.") + QByteArray::number(layer) + ".";
        add((prefix + "attn_ln.weight").constData(), {1, config.textStateSize});
        add((prefix + "attn_ln.bias").constData(), {1, config.textStateSize});
        add((prefix + "attn.query.weight").constData(), {config.textStateSize, config.textStateSize});
        add((prefix + "attn.query.bias").constData(), {1, config.textStateSize});
        add((prefix + "attn.key.weight").constData(), {config.textStateSize, config.textStateSize});
        add((prefix + "attn.value.weight").constData(), {config.textStateSize, config.textStateSize});
        add((prefix + "attn.value.bias").constData(), {1, config.textStateSize});
        add((prefix + "attn.out.weight").constData(), {config.textStateSize, config.textStateSize});
        add((prefix + "attn.out.bias").constData(), {1, config.textStateSize});
        add((prefix + "cross_attn_ln.weight").constData(), {1, config.textStateSize});
        add((prefix + "cross_attn_ln.bias").constData(), {1, config.textStateSize});
        add((prefix + "cross_attn.query.weight").constData(), {config.textStateSize, config.textStateSize});
        add((prefix + "cross_attn.query.bias").constData(), {1, config.textStateSize});
        add((prefix + "cross_attn.key.weight").constData(), {config.textStateSize, config.textStateSize});
        add((prefix + "cross_attn.value.weight").constData(), {config.textStateSize, config.textStateSize});
        add((prefix + "cross_attn.value.bias").constData(), {1, config.textStateSize});
        add((prefix + "cross_attn.out.weight").constData(), {config.textStateSize, config.textStateSize});
        add((prefix + "cross_attn.out.bias").constData(), {1, config.textStateSize});
        add((prefix + "mlp_ln.weight").constData(), {1, config.textStateSize});
        add((prefix + "mlp_ln.bias").constData(), {1, config.textStateSize});
        add((prefix + "mlp.0.weight").constData(), {textFfnSize, config.textStateSize});
        add((prefix + "mlp.0.bias").constData(), {1, textFfnSize});
        add((prefix + "mlp.2.weight").constData(), {config.textStateSize, textFfnSize});
        add((prefix + "mlp.2.bias").constData(), {1, config.textStateSize});
    }

    return tensors;
}

bool writeSyntheticMkcpur3Weights(const QString &path, const SyntheticWeightConfig &config)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    constexpr std::array<char, 8> magic{'M', 'K', 'C', 'P', 'U', 'R', '3', '\0'};
    const std::vector<TensorSpec> tensors = syntheticTensorSpecs(config);
    const QByteArray metadata = syntheticMetadataJson(config);
    const quint32 version = 3;
    const auto tensorCount = static_cast<quint32>(tensors.size());
    const auto metadataLen = static_cast<quint32>(metadata.size());

    if (file.write(magic.data(), static_cast<qint64>(magic.size())) != static_cast<qint64>(magic.size())
        || !writeScalar(&file, version) || !writeScalar(&file, tensorCount) || !writeScalar(&file, metadataLen)
        || file.write(metadata) != metadata.size()) {
        return false;
    }

    qint64 dataOffset = 0;
    for (const TensorSpec &tensor : tensors) {
        const auto nameLen = static_cast<quint32>(tensor.name.size());
        const auto dimCount = static_cast<quint32>(tensor.dims.size());
        const quint32 dtype = 0;
        const qint64 dataSize = tensorDataBytes(tensor);
        if (!writeScalar(&file, nameLen) || file.write(tensor.name) != tensor.name.size()
            || !writeScalar(&file, dimCount)) {
            return false;
        }
        for (const quint32 dim : tensor.dims) {
            if (!writeScalar(&file, dim)) {
                return false;
            }
        }
        if (!writeScalar(&file, dtype) || !writeScalar(&file, dataOffset) || !writeScalar(&file, dataSize)) {
            return false;
        }
        dataOffset += dataSize;
    }

    for (const TensorSpec &tensor : tensors) {
        const QByteArray tensorBytes(static_cast<qsizetype>(tensorDataBytes(tensor)), '\0');
        if (file.write(tensorBytes) != tensorBytes.size()) {
            return false;
        }
    }

    return true;
}

QString runtimeErrorText(const RuntimeError &error)
{
    return QStringLiteral("%1: %2").arg(error.message, error.detail);
}

QString normalizedTranscriptForAssertion(QString text)
{
    // Whisper vocab entries use GPT-2 byte-level markers. Convert the common
    // space/newline sentinels before stripping punctuation for phrase checks.
    text.replace(QChar(0x0120), QLatin1Char(' '));
    text.replace(QChar(0x010A), QLatin1Char(' '));
    text.replace(QStringLiteral("<|endoftext|>"), QLatin1String(" "));
    text.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9]+")), QStringLiteral(" "));
    return text.toLower().simplified();
}

bool containsJfkPhrase(const QString &transcript)
{
    return normalizedTranscriptForAssertion(transcript)
        .contains(QStringLiteral("ask not what your country can do for you"));
}

std::optional<std::vector<float>> loadJfkSamples(QString *error)
{
    const QString wavPath = QString::fromUtf8(MUTTERKEY_TEST_JFK_WAV_PATH);
    if (!QFileInfo::exists(wavPath)) {
        if (error != nullptr) {
            *error = QStringLiteral("JFK WAV fixture not found: %1").arg(wavPath);
        }
        return std::nullopt;
    }
    return readMono16kHzPcmWav(wavPath, error);
}

std::vector<int> loadSuppressedTokenIdsFromConfig(const QString &configPath)
{
    QFile configFile(configPath);
    if (!configFile.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(configFile.readAll());
    if (!document.isObject()) {
        return {};
    }

    std::vector<int> tokenIds;
    const QJsonArray tokens = document.object().value(QStringLiteral("suppress_tokens")).toArray();
    tokenIds.reserve(static_cast<std::size_t>(tokens.size()));
    for (const auto &value : tokens) {
        if (value.isDouble()) {
            tokenIds.push_back(value.toInt());
        }
    }
    return tokenIds;
}

CpuReferenceExecutionMetadata whisperBaseEnExecutionMetadata(std::vector<int> suppressedTokenIds = {})
{
    return CpuReferenceExecutionMetadata{
        .decoder = QStringLiteral("real-decoder-v3"),
        .tokenizer = QStringLiteral("whisper-bpe"),
        .baselineFamily = QStringLiteral("whisper-base-en"),
        .tokenizerAssetRole = QStringLiteral("tokenizer_vocab"),
        .tokenizerMergesAssetRole = QStringLiteral("tokenizer_merges"),
        .frontend = QStringLiteral("log-mel-v1"),
        .searchPolicy = QStringLiteral("greedy-real-v1"),
        .timestampMode = QStringLiteral("timestamp-token-v1"),
        .bosTokenId = 50257,
        .eosTokenId = 50256,
        .noSpeechTokenId = 50361,
        .timestampTokenStartId = 50363,
        .timestampTokenEndId = 51863,
        .initialPromptTokenIds = {50362},
        .suppressedTokenIds = std::move(suppressedTokenIds),
    };
}

std::optional<QString> decodeRealDecoderSamples(const std::vector<float> &samples,
                                                const std::shared_ptr<CpuWhisperModelWeights> &weights,
                                                const CpuWhisperTokenizerModel &tokenizer,
                                                const CpuReferenceExecutionMetadata &execution,
                                                QString *error)
{
    if (weights == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("Real decoder weights are not loaded");
        }
        return std::nullopt;
    }

    CpuReferenceModelData model;
    model.kind = CpuReferenceModelKind::RealDecoderV3;
    model.whisperTokenizer = tokenizer;
    model.tensorWeights = weights;

    CpuKVCache cache;
    cache.allocate(weights->config, weights->config.textContextSize);
    const std::optional<CpuDecodeResult> result = runCpuDecodePass(CpuDecodeRequest{
        .samples = samples,
        .model = &model,
        .execution = &execution,
        .kvCache = &cache,
        .sampleRate = 16000,
        .maxDecoderTokens = 96,
        .maxMelFrames = 1200,
    });
    if (!result.has_value()) {
        if (error != nullptr) {
            *error = QStringLiteral("Native real decoder returned no transcript");
        }
        return std::nullopt;
    }
    return result->transcript;
}

} // namespace

void CpuRealDecoderTest::initTestCase()
{
    const QString envPath =
        QProcessEnvironment::systemEnvironment().value(QStringLiteral("MUTTERKEY_TEST_WEIGHTS_PATH"));
    m_weightsPath = envPath;
    if (!m_weightsPath.isEmpty()) {
        m_modelDir = QFileInfo(m_weightsPath).absolutePath();
    }
    m_packagePath = QProcessEnvironment::systemEnvironment().value(QStringLiteral("MUTTERKEY_TEST_PACKAGE_PATH"));
}

void CpuRealDecoderTest::syntheticV3WeightsLoadWithDimensionValidation()
{
    // WHAT: Verify the MKCPUR3 loader accepts a complete synthetic tensor file under normal CTest.
    // HOW: Write a tiny but shape-complete V3 file and load it through the production tensor loader.
    // WHY: Phase 5B needs default synthetic coverage even when real model weights are unavailable locally.
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(true);
    QVERIFY(tempFile.open());
    tempFile.close();
    QVERIFY(writeSyntheticMkcpur3Weights(tempFile.fileName(), SyntheticWeightConfig{}));

    RuntimeError error;
    const auto weights = loadCpuWhisperModelWeights(tempFile.fileName(), &error);

    QVERIFY2(weights != nullptr, qPrintable(QStringLiteral("%1: %2").arg(error.message, error.detail)));
    QVERIFY(error.isOk());
    QCOMPARE(weights->config.melBands, 2);
    QCOMPARE(weights->config.audioStateSize, 2);
    QCOMPARE(weights->config.vocabularySize, 8);
}

void CpuRealDecoderTest::pipelineSmokeTest()
{
    // WHAT: Verify the real MKCPUR3 base.en weights load with the expected production dimensions.
    // HOW: Load the external weights from MUTTERKEY_TEST_WEIGHTS_PATH and inspect the parsed tensor metadata.
    // WHY: The full real-audio conformance tests below cover decoding; this smoke stays cheap enough for local debug builds.

    if (m_weightsPath.isEmpty()) {
        QSKIP("MUTTERKEY_TEST_WEIGHTS_PATH not set — skipping real decoder pipeline test");
    }
    if (!QFileInfo::exists(m_weightsPath)) {
        QSKIP(qPrintable(QStringLiteral("Weights file not found: %1").arg(m_weightsPath)));
    }

    RuntimeError loadError;
    const auto weights = loadCpuWhisperModelWeights(m_weightsPath, &loadError);
    if (weights == nullptr) {
        QFAIL(qPrintable(QStringLiteral("Failed to load weights: %1 — %2").arg(loadError.message, loadError.detail)));
        return;
    }

    QCOMPARE(weights->config.audioStateSize, 512);
    QCOMPARE(weights->config.vocabularySize, 51864);
}

void CpuRealDecoderTest::pipelineTranscribesJfkSample()
{
    // WHAT: Verify the real native CPU decoder can transcribe the committed JFK speech fixture with real base.en weights.
    // HOW: Load MKCPUR3 weights plus tokenizer assets from the staging directory, decode jfk.wav, and assert both raw and normalized canonical phrases.
    // WHY: Phase 5B needs real short-utterance evidence, not only synthetic tensors or a sine-wave smoke path.
    if (m_weightsPath.isEmpty()) {
        QSKIP("MUTTERKEY_TEST_WEIGHTS_PATH not set — skipping real JFK decoder test");
    }
    if (!QFileInfo::exists(m_weightsPath)) {
        QSKIP(qPrintable(QStringLiteral("Weights file not found: %1").arg(m_weightsPath)));
    }

    QString fixtureError;
    const std::optional<std::vector<float>> samples = loadJfkSamples(&fixtureError);
    if (!samples.has_value()) {
        QFAIL(qPrintable(fixtureError));
        return;
    }
    const std::vector<float> &loadedSamples = samples.value();

    RuntimeError loadError;
    const std::shared_ptr<CpuWhisperModelWeights> weights = loadCpuWhisperModelWeights(m_weightsPath, &loadError);
    QVERIFY2(weights != nullptr, qPrintable(runtimeErrorText(loadError)));

    const auto tokenizer = loadTokenizerFromHuggingFace(TokenizerPaths{
        .vocabPath = m_modelDir + QStringLiteral("/vocab.json"),
        .mergesPath = m_modelDir + QStringLiteral("/merges.txt"),
    });
    if (!tokenizer.has_value()) {
        QFAIL("Failed to load tokenizer from staging directory");
        return;
    }
    const CpuWhisperTokenizerModel &loadedTokenizer = tokenizer.value();

    std::vector<int> suppressedTokenIds =
        loadSuppressedTokenIdsFromConfig(m_modelDir + QStringLiteral("/config.json"));
    QVERIFY2(!suppressedTokenIds.empty(), "Failed to load suppress_tokens from staging config.json");

    QString decodeError;
    const std::optional<QString> transcript = decodeRealDecoderSamples(loadedSamples,
                                                                        weights,
                                                                        loadedTokenizer,
                                                                        whisperBaseEnExecutionMetadata(std::move(suppressedTokenIds)),
                                                                        &decodeError);
    if (!transcript.has_value()) {
        QFAIL(qPrintable(decodeError));
        return;
    }
    const QString &decodedTranscript = transcript.value();
    qDebug().noquote() << "Native JFK transcript:" << decodedTranscript;
    QVERIFY2(containsJfkPhrase(decodedTranscript),
             qPrintable(QStringLiteral("Transcript did not contain JFK phrase after normalization: %1")
                            .arg(normalizedTranscriptForAssertion(decodedTranscript))));
    QVERIFY2(decodedTranscript.contains(QStringLiteral("And so my fellow Americans")),
             qPrintable(QStringLiteral("Transcript did not contain the clean JFK opening: %1").arg(decodedTranscript)));
    QVERIFY2(!decodedTranscript.contains(QChar(0x0120)),
             qPrintable(QStringLiteral("Transcript leaked byte-level space markers: %1").arg(decodedTranscript)));
}

void CpuRealDecoderTest::pipelineLoadsStagedPackage()
{
    // WHAT: Verify a fully staged native package validates, loads, and decodes the JFK fixture through the production package path.
    // HOW: Resolve MUTTERKEY_TEST_PACKAGE_PATH with ModelValidator, load CpuReferenceModelHandle, then run one real decode pass.
    // WHY: The staging helper must produce packages consumed by the runtime loader, not just raw weights accepted by isolated tests.
    if (m_packagePath.isEmpty()) {
        QSKIP("MUTTERKEY_TEST_PACKAGE_PATH not set — skipping staged-package decoder test");
    }
    if (!QFileInfo::exists(m_packagePath)) {
        QSKIP(qPrintable(QStringLiteral("Package path not found: %1").arg(m_packagePath)));
    }

    QString fixtureError;
    const std::optional<std::vector<float>> samples = loadJfkSamples(&fixtureError);
    if (!samples.has_value()) {
        QFAIL(qPrintable(fixtureError));
        return;
    }
    const std::vector<float> &loadedSamples = samples.value();

    RuntimeError validationError;
    const std::optional<ValidatedModelPackage> package =
        ModelValidator::validatePackagePath(m_packagePath, cpuReferenceEngineName(), cpuReferenceModelFormat(), &validationError);
    if (!package.has_value()) {
        QFAIL(qPrintable(runtimeErrorText(validationError)));
        return;
    }
    const ValidatedModelPackage &validatedPackage = package.value();
    QCOMPARE(validatedPackage.nativeExecution().decoder, QStringLiteral("real-decoder-v3"));
    QCOMPARE(validatedPackage.nativeExecution().initialPromptTokenIds.size(), std::size_t{1});
    QVERIFY(!validatedPackage.nativeExecution().suppressedTokenIds.empty());

    RuntimeError loadError;
    const std::shared_ptr<const CpuReferenceModelHandle> handle = loadCpuReferenceModelHandle(validatedPackage, &loadError);
    QVERIFY2(handle != nullptr, qPrintable(runtimeErrorText(loadError)));
    QCOMPARE(handle->model().kind, CpuReferenceModelKind::RealDecoderV3);
    QVERIFY(handle->model().tensorWeights != nullptr);
    QVERIFY(handle->model().whisperTokenizer.has_value());

    CpuKVCache cache;
    cache.allocate(handle->model().tensorWeights->config, handle->model().tensorWeights->config.textContextSize);
    const std::optional<CpuDecodeResult> result = runCpuDecodePass(CpuDecodeRequest{
        .samples = loadedSamples,
        .model = &handle->model(),
        .execution = &handle->execution(),
        .kvCache = &cache,
        .sampleRate = 16000,
        .maxDecoderTokens = 96,
        .maxMelFrames = 1200,
    });

    if (!result.has_value()) {
        QFAIL("Staged package produced no JFK transcript");
        return;
    }
    const CpuDecodeResult &decodeResult = result.value();
    qDebug().noquote() << "Staged package JFK transcript:" << decodeResult.transcript;
    QVERIFY2(containsJfkPhrase(decodeResult.transcript),
             qPrintable(QStringLiteral("Package transcript did not contain JFK phrase after normalization: %1")
                            .arg(normalizedTranscriptForAssertion(decodeResult.transcript))));
    QVERIFY2(decodeResult.transcript.contains(QStringLiteral("And so my fellow Americans")),
             qPrintable(QStringLiteral("Package transcript did not contain the clean JFK opening: %1").arg(decodeResult.transcript)));
    QVERIFY2(!decodeResult.transcript.contains(QChar(0x0120)),
             qPrintable(QStringLiteral("Package transcript leaked byte-level space markers: %1").arg(decodeResult.transcript)));
}

void CpuRealDecoderTest::dimensionValidationRejectsWrongShape()
{
    // WHAT: Verify that loadCpuWhisperModelWeights rejects a file with wrong tensor dimensions.
    // HOW: Create a complete MKCPUR3 file whose encoder conv tensor has a deliberately wrong shape.
    // WHY: Dimension validation is a security and correctness requirement and must run without external weights.
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(true);
    QVERIFY(tempFile.open());
    tempFile.close();
    SyntheticWeightConfig config;
    config.corruptConv1Shape = true;
    QVERIFY(writeSyntheticMkcpur3Weights(tempFile.fileName(), config));

    RuntimeError error;
    const auto weights = loadCpuWhisperModelWeights(tempFile.fileName(), &error);

    QVERIFY2(weights == nullptr, "Expected load to fail for dimensionally wrong weights");
    QCOMPARE(error.code, RuntimeErrorCode::ModelLoadFailed);
    QCOMPARE(error.message, QStringLiteral("Weight dimension mismatch"));
    QVERIFY(error.detail.contains(QStringLiteral("encoder.conv1.weight")));
}

QTEST_APPLESS_MAIN(CpuRealDecoderTest)

#include "cpurealdecodertest.moc"
