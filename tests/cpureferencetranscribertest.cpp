#include "transcription/cpufeatureextractor.h"
#include "transcription/cpureferencetranscriber.h"
#include "transcription/modelpackage.h"
#include "transcription/transcriptionengine.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::array<char, 8> kCpuReferenceMagicV2{'M', 'K', 'C', 'P', 'U', 'R', '2', '\0'};
constexpr std::uint32_t kCpuReferenceVersion2 = 2;
constexpr int kFeatureBinCount = 12;
constexpr int kSamplesPerBin = 128;

class CpuReferenceTranscriberTest final : public QObject
{
    Q_OBJECT

private slots:
    void engineDispatchesToNativeCpuPackage();
    void baselineFamilyPackageLoadsAsExplicitDecoderKind();
    void nativeCpuRuntimeMatchesClosestTemplate();
    void nativeCpuRuntimeRejectsTranslationMode();
    void nativeCpuRuntimeTreatsLowEnergyInputAsNoSpeech();
    void engineDiagnosticsExplainNativeSelection();
};

template <typename T>
bool writePod(QFile *file, const T &value)
{
    if (file == nullptr) {
        return false;
    }

    QByteArray bytes(static_cast<qsizetype>(sizeof(T)), '\0');
    std::memcpy(bytes.data(), &value, sizeof(T));
    return file->write(bytes) == bytes.size();
}

struct CpuReferenceTemplatePhrase {
    QString text;
    std::vector<float> profile;
};

struct CpuReferenceFixturePackage {
    QString packageRoot;
    std::vector<CpuReferenceTemplatePhrase> phrases;
    float maxDistance = 0.35F;
    bool baselineFamily = false;
};

QString sha256ForFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}

std::vector<float> makeNormalizedProfile(std::initializer_list<float> values)
{
    const std::vector<float> raw(values);
    return extractCpuEnergyProfile(raw, static_cast<int>(raw.size()));
}

std::vector<float> synthesizeSamples(const std::vector<float> &profile)
{
    std::vector<float> samples;
    samples.reserve(profile.size() * static_cast<std::size_t>(kSamplesPerBin));
    std::size_t binIndex = 0;
    for (const float amplitude : profile) {
        for (int sampleIndex = 0; sampleIndex < kSamplesPerBin; ++sampleIndex) {
            const float sign = ((sampleIndex + static_cast<int>(binIndex)) % 2 == 0) ? 1.0F : -1.0F;
            samples.push_back(amplitude * sign);
        }
        ++binIndex;
    }
    return samples;
}

bool writeCpuReferenceModel(const QString &path, const std::vector<CpuReferenceTemplatePhrase> &phrases, float maxDistance)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    if (phrases.empty()) {
        return false;
    }

    if (file.write(kCpuReferenceMagicV2.data(), static_cast<qint64>(kCpuReferenceMagicV2.size()))
        != static_cast<qint64>(kCpuReferenceMagicV2.size())) {
        return false;
    }

    const auto featureBinCount = static_cast<std::uint32_t>(phrases.front().profile.size());
    const auto phraseCount = static_cast<std::uint32_t>(phrases.size());
    const auto maxDistanceMilli = static_cast<std::uint32_t>(std::lround(maxDistance * 1000.0F));
    if (!writePod(&file, kCpuReferenceVersion2) || !writePod(&file, featureBinCount) || !writePod(&file, phraseCount)
        || !writePod(&file, maxDistanceMilli)) {
        return false;
    }

    for (const CpuReferenceTemplatePhrase &phrase : phrases) {
        if (phrase.text.trimmed().isEmpty() || phrase.profile.size() != static_cast<std::size_t>(featureBinCount)) {
            return false;
        }

        const QByteArray textUtf8 = phrase.text.toUtf8();
        const auto textBytes = static_cast<std::uint32_t>(textUtf8.size());
        if (!writePod(&file, textBytes) || !writePod(&file, featureBinCount)) {
            return false;
        }
        if (file.write(textUtf8) != textUtf8.size()) {
            return false;
        }
        for (const float value : phrase.profile) {
            if (!writePod(&file, value)) {
                return false;
            }
        }
    }

    return true;
}

bool writeCpuReferencePackage(const CpuReferenceFixturePackage &request)
{
    const QDir root;
    if (!root.mkpath(QDir(request.packageRoot).filePath(QStringLiteral("assets")))) {
        return false;
    }

    const QString weightsPath = QDir(request.packageRoot).filePath(QStringLiteral("assets/model.mkcpu"));
    if (!writeCpuReferenceModel(weightsPath, request.phrases, request.maxDistance)) {
        return false;
    }
    const QString tokenizerPath = QDir(request.packageRoot).filePath(QStringLiteral("assets/tokenizer.txt"));
    QFile tokenizerFile(tokenizerPath);
    if (!tokenizerFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    tokenizerFile.write(request.baselineFamily ? "h\ne\nl\no\np\nn\no\nr\nhello\nopen\neditor\n"
                                               : "hello\nfrom\ncpu\nruntime\nopen\neditor\n");
    tokenizerFile.close();
    const QString tokenizerMergesPath = QDir(request.packageRoot).filePath(QStringLiteral("assets/tokenizer.merges"));
    if (request.baselineFamily) {
        QFile tokenizerMergesFile(tokenizerMergesPath);
        if (!tokenizerMergesFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        tokenizerMergesFile.write("h e\nhe l\nhel l\no p\nop e\nope n\n");
        tokenizerMergesFile.close();
    }

    ModelPackageManifest manifest;
    manifest.format = QStringLiteral("mutterkey.model-package");
    manifest.schemaVersion = 1;
    manifest.metadata.packageId = QStringLiteral("fixture-native-cpu");
    manifest.metadata.displayName = QStringLiteral("Fixture Native CPU");
    manifest.metadata.runtimeFamily = QStringLiteral("asr");
    manifest.metadata.sourceFormat = QStringLiteral("mutterkey-native-decoder");
    manifest.metadata.modelFormat = cpuReferenceModelFormat();
    manifest.metadata.architecture = request.baselineFamily ? QStringLiteral("whisper-base-en")
                                                           : QStringLiteral("template-decoder");
    manifest.metadata.languageProfile = QStringLiteral("en");
    manifest.metadata.tokenizer = request.baselineFamily ? QStringLiteral("openai-whisper-bpe-v1")
                                                         : QStringLiteral("phrase-template");
    manifest.metadata.vocabularySize = request.baselineFamily ? 11 : 6;
    manifest.metadata.melCount = 80;
    manifest.nativeExecution = NativeExecutionMetadata{
        .executionVersion = request.baselineFamily ? 2 : 1,
        .baselineFamily = request.baselineFamily ? QStringLiteral("whisper-base-en") : QString{},
        .decoder = QStringLiteral("template-matcher"),
        .tokenizer = request.baselineFamily ? QStringLiteral("openai-whisper-bpe-v1")
                                            : QStringLiteral("phrase-template"),
        .tokenizerAssetRole = QStringLiteral("tokenizer"),
        .tokenizerMergesAssetRole = request.baselineFamily ? QStringLiteral("tokenizer_merges") : QString{},
        .frontend = QStringLiteral("energy-profile-v1"),
        .searchPolicy = QStringLiteral("greedy-template-v1"),
        .timestampMode = request.baselineFamily ? QStringLiteral("timestamp-token-v1")
                                               : QStringLiteral("utterance-duration-v1"),
        .featureBinCount = static_cast<int>(request.phrases.front().profile.size()),
        .templateCount = static_cast<int>(request.phrases.size()),
        .maxDistance = request.maxDistance,
        .bosTokenId = 0,
        .eosTokenId = 1,
        .noSpeechTokenId = 2,
        .timestampTokenStartId = 3,
        .timestampTokenEndId = 5,
    };
    manifest.compatibleEngines.push_back(ModelCompatibilityMarker{
        .engine = cpuReferenceEngineName(),
        .modelFormat = cpuReferenceModelFormat(),
    });
    manifest.assets.push_back(ModelAssetMetadata{
        .role = QStringLiteral("weights"),
        .relativePath = QStringLiteral("assets/model.mkcpu"),
        .sha256 = sha256ForFile(weightsPath),
        .sizeBytes = QFileInfo(weightsPath).size(),
    });
    manifest.assets.push_back(ModelAssetMetadata{
        .role = QStringLiteral("tokenizer"),
        .relativePath = QStringLiteral("assets/tokenizer.txt"),
        .sha256 = sha256ForFile(tokenizerPath),
        .sizeBytes = QFileInfo(tokenizerPath).size(),
    });
    if (request.baselineFamily) {
        manifest.assets.push_back(ModelAssetMetadata{
            .role = QStringLiteral("tokenizer_merges"),
            .relativePath = QStringLiteral("assets/tokenizer.merges"),
            .sha256 = sha256ForFile(tokenizerMergesPath),
            .sizeBytes = QFileInfo(tokenizerMergesPath).size(),
        });
    }

    QFile manifestFile(QDir(request.packageRoot).filePath(QStringLiteral("model.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    manifestFile.write(QJsonDocument(modelPackageManifestToJson(manifest)).toJson(QJsonDocument::Indented));
    return true;
}

AudioChunk validChunkForProfile(const std::vector<float> &profile)
{
    return AudioChunk{
        .samples = synthesizeSamples(profile),
        .sampleRate = 16000,
        .channels = 1,
        .streamOffsetFrames = 0,
    };
}

std::vector<CpuReferenceTemplatePhrase> templatePhrases()
{
    return {
        CpuReferenceTemplatePhrase{
            .text = QStringLiteral("hello from cpu runtime"),
            .profile = makeNormalizedProfile({0.05F, 0.12F, 0.28F, 0.60F, 0.85F, 0.55F, 0.32F, 0.18F, 0.12F, 0.09F, 0.05F, 0.03F}),
        },
        CpuReferenceTemplatePhrase{
            .text = QStringLiteral("open editor"),
            .profile = makeNormalizedProfile({0.82F, 0.72F, 0.18F, 0.10F, 0.12F, 0.16F, 0.44F, 0.78F, 0.64F, 0.22F, 0.10F, 0.06F}),
        },
    };
}

} // namespace

void CpuReferenceTranscriberTest::engineDispatchesToNativeCpuPackage()
{
    // WHAT: Verify that the generic engine factory chooses the native CPU runtime for a native package.
    // HOW: Create a temporary package marked for the product-owned CPU runtime and inspect the backend name exposed by the factory.
    // WHY: Phase 5B only starts shrinking the whisper.cpp dependency once the app-level factory still routes native packages through the product-owned runtime seam.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString packageRoot = tempDir.filePath(QStringLiteral("native-package"));
    QVERIFY(writeCpuReferencePackage(CpuReferenceFixturePackage{
        .packageRoot = packageRoot,
        .phrases = templatePhrases(),
    }));

    TranscriberConfig config;
    config.modelPath = packageRoot;

    const std::shared_ptr<const TranscriptionEngine> engine = createTranscriptionEngine(config);
    QVERIFY(engine != nullptr);
    QCOMPARE(engine->capabilities().backendName, cpuReferenceEngineName());
}

void CpuReferenceTranscriberTest::baselineFamilyPackageLoadsAsExplicitDecoderKind()
{
    // WHAT: Verify that a baseline-family native package no longer loads as the template-scaffold kind.
    // HOW: Create a temporary baseline-family package and inspect the resolved native model handle.
    // WHY: The second-pass Phase 5B split should keep scaffold and future real-decoder lanes explicit in the loaded-model state.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString packageRoot = tempDir.filePath(QStringLiteral("native-baseline-package"));
    QVERIFY(writeCpuReferencePackage(CpuReferenceFixturePackage{
        .packageRoot = packageRoot,
        .phrases = templatePhrases(),
        .baselineFamily = true,
    }));

    TranscriberConfig config;
    config.modelPath = packageRoot;

    RuntimeError error;
    const std::shared_ptr<const TranscriptionModelHandle> genericModel = CpuReferenceTranscriber::loadModelHandle(config, &error);
    QVERIFY(genericModel != nullptr);
    QVERIFY(error.isOk());

    const std::shared_ptr<const CpuReferenceModelHandle> model = resolveCpuReferenceModelHandle(genericModel);
    QVERIFY(model != nullptr);
    QCOMPARE(static_cast<int>(model->model().kind), static_cast<int>(CpuReferenceModelKind::BaselineFamilyDecoderV2));
    QVERIFY(model->model().whisperTokenizer.has_value());
}

void CpuReferenceTranscriberTest::nativeCpuRuntimeMatchesClosestTemplate()
{
    // WHAT: Verify that the native CPU runtime chooses the closest phrase template from audio-derived features.
    // HOW: Build a temporary native package with two phrase templates, synthesize audio from the second profile, and finish a live session.
    // WHY: This proves the native runtime now depends on model data plus audio features instead of emitting a baked-in transcript fixture regardless of the input audio.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString packageRoot = tempDir.filePath(QStringLiteral("native-package"));
    const std::vector<CpuReferenceTemplatePhrase> phrases = templatePhrases();
    QVERIFY(writeCpuReferencePackage(CpuReferenceFixturePackage{
        .packageRoot = packageRoot,
        .phrases = phrases,
    }));

    TranscriberConfig config;
    config.modelPath = packageRoot;

    RuntimeError error;
    const std::shared_ptr<const TranscriptionModelHandle> model = CpuReferenceTranscriber::loadModelHandle(config, &error);
    QVERIFY(model != nullptr);
    QVERIFY(error.isOk());

    std::unique_ptr<TranscriptionSession> session = CpuReferenceTranscriber::createSession(config, model);
    QVERIFY(session != nullptr);
    QVERIFY(session->warmup(&error));
    QVERIFY(error.isOk());

    const TranscriptUpdate pushUpdate = session->pushAudioChunk(validChunkForProfile(phrases.back().profile));
    QVERIFY(pushUpdate.isOk());

    const TranscriptUpdate finishUpdate = session->finish();
    QVERIFY(finishUpdate.isOk());
    QCOMPARE(finishUpdate.events.size(), static_cast<std::size_t>(1));
    QCOMPARE(finishUpdate.events.front().kind, TranscriptEventKind::Final);
    QCOMPARE(finishUpdate.events.front().text, QStringLiteral("open editor"));
    QCOMPARE(finishUpdate.events.front().startMs, 0);
    QVERIFY(finishUpdate.events.front().endMs > 0);
}

void CpuReferenceTranscriberTest::nativeCpuRuntimeRejectsTranslationMode()
{
    // WHAT: Verify that the native CPU runtime rejects translation mode in its narrow Phase 5 scope.
    // HOW: Create a temporary native package, enable translate in config, and finish a session after pushing one valid chunk.
    // WHY: The first product-owned CPU runtime is intentionally ASR-only, so unsupported feature requests must fail explicitly instead of silently diverging from the contract.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString packageRoot = tempDir.filePath(QStringLiteral("native-package"));
    const std::vector<CpuReferenceTemplatePhrase> phrases = templatePhrases();
    QVERIFY(writeCpuReferencePackage(CpuReferenceFixturePackage{
        .packageRoot = packageRoot,
        .phrases = phrases,
    }));

    TranscriberConfig config;
    config.modelPath = packageRoot;
    config.translate = true;

    RuntimeError error;
    const std::shared_ptr<const TranscriptionModelHandle> model = CpuReferenceTranscriber::loadModelHandle(config, &error);
    QVERIFY(model != nullptr);
    QVERIFY(error.isOk());

    std::unique_ptr<TranscriptionSession> session = CpuReferenceTranscriber::createSession(config, model);
    QVERIFY(session != nullptr);
    QVERIFY(session->pushAudioChunk(validChunkForProfile(phrases.front().profile)).isOk());

    const TranscriptUpdate finishUpdate = session->finish();
    QVERIFY(!finishUpdate.isOk());
    QCOMPARE(finishUpdate.error.code, RuntimeErrorCode::InvalidConfig);
}

void CpuReferenceTranscriberTest::nativeCpuRuntimeTreatsLowEnergyInputAsNoSpeech()
{
    // WHAT: Verify that the native CPU runtime does not emit a transcript for effectively silent input.
    // HOW: Push a valid 16 kHz mono chunk whose samples stay below the runtime speech floor, then finish the session.
    // WHY: The native path should preserve the existing short-utterance silence rejection behavior while the decoder internals evolve.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString packageRoot = tempDir.filePath(QStringLiteral("native-package"));
    QVERIFY(writeCpuReferencePackage(CpuReferenceFixturePackage{
        .packageRoot = packageRoot,
        .phrases = templatePhrases(),
    }));

    TranscriberConfig config;
    config.modelPath = packageRoot;

    RuntimeError error;
    const std::shared_ptr<const TranscriptionModelHandle> model = CpuReferenceTranscriber::loadModelHandle(config, &error);
    QVERIFY(model != nullptr);
    QVERIFY(error.isOk());

    std::unique_ptr<TranscriptionSession> session = CpuReferenceTranscriber::createSession(config, model);
    QVERIFY(session != nullptr);

    AudioChunk silenceChunk;
    silenceChunk.samples.assign(static_cast<std::size_t>(kFeatureBinCount) * static_cast<std::size_t>(kSamplesPerBin), 0.005F);
    silenceChunk.sampleRate = 16000;
    silenceChunk.channels = 1;

    const TranscriptUpdate pushUpdate = session->pushAudioChunk(silenceChunk);
    QVERIFY(pushUpdate.isOk());

    const TranscriptUpdate finishUpdate = session->finish();
    QVERIFY(finishUpdate.isOk());
    QVERIFY(finishUpdate.events.empty());
}

void CpuReferenceTranscriberTest::engineDiagnosticsExplainNativeSelection()
{
    // WHAT: Verify that native-runtime diagnostics explain why the generic factory chose the native backend.
    // HOW: Create a temporary native package, construct the generic engine, and inspect the selection reason surfaced in runtime diagnostics.
    // WHY: Once runtime policy is moved out of the factory body, diagnostics should preserve that policy decision so diagnose output remains actionable.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString packageRoot = tempDir.filePath(QStringLiteral("native-package"));
    QVERIFY(writeCpuReferencePackage(CpuReferenceFixturePackage{
        .packageRoot = packageRoot,
        .phrases = templatePhrases(),
    }));

    TranscriberConfig config;
    config.modelPath = packageRoot;

    const std::shared_ptr<const TranscriptionEngine> engine = createTranscriptionEngine(config);
    QVERIFY(engine != nullptr);
    QCOMPARE(engine->diagnostics().backendName, cpuReferenceEngineName());
    QVERIFY(engine->diagnostics().selectionReason.contains(QStringLiteral("native CPU decoder runtime"),
                                                          Qt::CaseInsensitive));
}

QTEST_APPLESS_MAIN(CpuReferenceTranscriberTest)

#include "cpureferencetranscribertest.moc"
