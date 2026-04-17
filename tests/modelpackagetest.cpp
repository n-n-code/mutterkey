#include "asr/model/modelcatalog.h"
#include "asr/model/modelpackage.h"
#include "asr/model/modelvalidator.h"
#include "asr/model/rawwhisperimporter.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <array>
#include <cstring>

namespace {

class ModelPackageTest final : public QObject
{
    Q_OBJECT

private slots:
    void validatorAcceptsWellFormedPackage();
    void validatorRejectsHashMismatch();
    void validatorRejectsNativeDecoderPackageWithoutExecutionMetadata();
    void validatorRejectsNativeDecoderPackageWithoutTokenizerAsset();
    void validatorRejectsBaselineDecoderPackageWithoutTokenizerMerges();
    void catalogSupportsLegacyRawWhisperCompatibility();
    void importerCreatesNativePackageFromRawWhisperFile();
    void nativeExecutionJsonRoundTripsInitialPromptTokens();
};

template <typename T>
bool writePaddedValue(QFile *file, const T &value)
{
    if (file == nullptr) {
        return false;
    }

    QByteArray bytes(static_cast<qsizetype>(sizeof(T)), '\0');
    std::memcpy(bytes.data(), &value, sizeof(T));
    return file->write(bytes) == bytes.size();
}

bool writeRawWhisperFixture(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    const quint32 magic = 0x67676d6cU;
    const std::array<qint32, 11> values{
        51864, // n_vocab
        1500,  // n_audio_ctx
        384,   // n_audio_state
        6,     // n_audio_head
        6,     // n_audio_layer
        448,   // n_text_ctx
        384,   // n_text_state
        6,     // n_text_head
        6,     // n_text_layer
        80,    // n_mels
        1,     // ftype
    };

    if (!writePaddedValue(&file, magic)) {
        return false;
    }
    for (const qint32 value : values) {
        if (!writePaddedValue(&file, value)) {
            return false;
        }
    }
    return file.write("fixture-whisper-weights", 23) == 23;
}

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

struct PackageFixtureRequest {
    QString packageRoot;
    QString weightsPayload;
    QString hashOverride;
    bool nativeDecoder = false;
    bool baselineTokenizer = false;
};

bool writePackage(const PackageFixtureRequest &request)
{
    const QDir root;
    if (!root.mkpath(QDir(request.packageRoot).filePath(QStringLiteral("assets")))) {
        return false;
    }

    const QString weightsPath = QDir(request.packageRoot).filePath(QStringLiteral("assets/model.bin"));
    QFile weightsFile(weightsPath);
    if (!weightsFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    weightsFile.write(request.weightsPayload.toUtf8());
    weightsFile.close();

    const QString tokenizerPath = QDir(request.packageRoot).filePath(QStringLiteral("assets/tokenizer.txt"));
    QFile tokenizerFile(tokenizerPath);
    if (!tokenizerFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    if (request.baselineTokenizer) {
        tokenizerFile.write("h\ne\nl\no\nw\nwo\nr\nrl\nd\n");
    } else {
        tokenizerFile.write("hello\nfrom\ncpu\nruntime\nopen\neditor\n");
    }
    tokenizerFile.close();

    const QString mergesPath = QDir(request.packageRoot).filePath(QStringLiteral("assets/tokenizer.merges"));
    if (request.baselineTokenizer) {
        QFile mergesFile(mergesPath);
        if (!mergesFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        mergesFile.write("h e\nhe l\nhel l\nhell o\nw o\nwo r\nwor l\nworl d\n");
        mergesFile.close();
    }

    ModelPackageManifest manifest;
    manifest.format = QStringLiteral("mutterkey.model-package");
    manifest.schemaVersion = 1;
    manifest.metadata.packageId = QStringLiteral("fixture-base-en");
    manifest.metadata.displayName = QStringLiteral("Fixture Base EN");
    manifest.metadata.runtimeFamily = QStringLiteral("asr");
    manifest.metadata.sourceFormat = QStringLiteral("whisper.cpp-ggml");
    manifest.metadata.modelFormat = QStringLiteral("ggml");
    manifest.metadata.architecture = QStringLiteral("base");
    manifest.metadata.languageProfile = QStringLiteral("en");
    manifest.metadata.quantization = QStringLiteral("float16");
    manifest.metadata.tokenizer = QStringLiteral("embedded");
    if (request.nativeDecoder) {
        manifest.metadata.sourceFormat = QStringLiteral("mutterkey-native-decoder");
        manifest.metadata.modelFormat = cpuReferenceModelFormat();
        manifest.metadata.architecture = QStringLiteral("template-decoder");
        manifest.metadata.tokenizer = request.baselineTokenizer ? QStringLiteral("openai-whisper-bpe-v1")
                                                               : QStringLiteral("phrase-template");
        manifest.metadata.vocabularySize = request.baselineTokenizer ? 9 : 6;
        manifest.metadata.melCount = 80;
        manifest.nativeExecution = NativeExecutionMetadata{
            .executionVersion = request.baselineTokenizer ? 2 : 1,
            .baselineFamily = request.baselineTokenizer ? QStringLiteral("whisper-base-en") : QString{},
            .decoder = QStringLiteral("template-matcher"),
            .tokenizer = request.baselineTokenizer ? QStringLiteral("openai-whisper-bpe-v1")
                                                   : QStringLiteral("phrase-template"),
            .tokenizerAssetRole = QStringLiteral("tokenizer"),
            .tokenizerMergesAssetRole = request.baselineTokenizer ? QStringLiteral("tokenizer_merges") : QString{},
            .frontend = QStringLiteral("energy-profile-v1"),
            .searchPolicy = QStringLiteral("greedy-template-v1"),
            .timestampMode = request.baselineTokenizer ? QStringLiteral("timestamp-token-v1")
                                                       : QStringLiteral("utterance-duration-v1"),
            .featureBinCount = 12,
            .templateCount = 2,
            .maxDistance = 0.35,
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
    } else {
        manifest.compatibleEngines.push_back(ModelCompatibilityMarker{
            .engine = QStringLiteral("whisper.cpp"),
            .modelFormat = QStringLiteral("ggml"),
        });
    }
    manifest.assets.push_back(ModelAssetMetadata{
        .role = QStringLiteral("weights"),
        .relativePath = QStringLiteral("assets/model.bin"),
        .sha256 = request.hashOverride.isEmpty() ? sha256ForFile(weightsPath) : request.hashOverride,
        .sizeBytes = QFileInfo(weightsPath).size(),
    });
    if (request.nativeDecoder) {
        manifest.assets.push_back(ModelAssetMetadata{
            .role = QStringLiteral("tokenizer"),
            .relativePath = QStringLiteral("assets/tokenizer.txt"),
            .sha256 = sha256ForFile(tokenizerPath),
            .sizeBytes = QFileInfo(tokenizerPath).size(),
        });
        if (request.baselineTokenizer) {
            manifest.assets.push_back(ModelAssetMetadata{
                .role = QStringLiteral("tokenizer_merges"),
                .relativePath = QStringLiteral("assets/tokenizer.merges"),
                .sha256 = sha256ForFile(mergesPath),
                .sizeBytes = QFileInfo(mergesPath).size(),
            });
        }
    }

    QFile manifestFile(QDir(request.packageRoot).filePath(QStringLiteral("model.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    manifestFile.write(QJsonDocument(modelPackageManifestToJson(manifest)).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace

void ModelPackageTest::validatorAcceptsWellFormedPackage()
{
    // WHAT: Verify that a well-formed native model package passes validation.
    // HOW: Create a temporary package directory with a valid manifest, weights file, and hash,
    // then validate it for the whisper.cpp / ggml runtime markers.
    // WHY: The product-owned package gate only helps if valid packages can be accepted before
    // inference begins while malformed ones are rejected deterministically.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString packageRoot = tempDir.filePath(QStringLiteral("fixture-package"));
    QVERIFY(writePackage(PackageFixtureRequest{
        .packageRoot = packageRoot,
        .weightsPayload = QStringLiteral("fixture weights"),
    }));

    RuntimeError error;
    const std::optional<ValidatedModelPackage> package =
        ModelValidator::validatePackagePath(packageRoot, QStringLiteral("whisper.cpp"), QStringLiteral("ggml"), &error);

    if (!package.has_value()) {
        QFAIL("Expected validated package");
        return;
    }
    QVERIFY(error.isOk());
    const ValidatedModelPackage &validatedPackage = *package;
    QCOMPARE(validatedPackage.metadata().packageId, QStringLiteral("fixture-base-en"));
    QCOMPARE(validatedPackage.weightsPath, QDir(packageRoot).filePath(QStringLiteral("assets/model.bin")));
}

void ModelPackageTest::validatorRejectsHashMismatch()
{
    // WHAT: Verify that the package validator rejects a manifest whose hash does not match the weights asset.
    // HOW: Create a normal package but force the manifest SHA-256 to an incorrect value, then validate it.
    // WHY: Integrity checks are the main reason Phase 4 exists, so a mismatched asset hash must fail
    // before the runtime can hand the file to whisper.cpp.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString packageRoot = tempDir.filePath(QStringLiteral("broken-package"));
    QVERIFY(writePackage(PackageFixtureRequest{
        .packageRoot = packageRoot,
        .weightsPayload = QStringLiteral("fixture weights"),
        .hashOverride = QStringLiteral("deadbeef"),
    }));

    RuntimeError error;
    const std::optional<ValidatedModelPackage> package = ModelValidator::validatePackagePath(packageRoot, {}, {}, &error);

    QVERIFY(!package.has_value());
    QCOMPARE(error.code, RuntimeErrorCode::ModelIntegrityFailed);
}

void ModelPackageTest::validatorRejectsNativeDecoderPackageWithoutExecutionMetadata()
{
    // WHAT: Verify that mkasr-v2 native decoder packages must declare native execution metadata in the manifest.
    // HOW: Create a temporary native-decoder package, then remove the execution metadata block before validation.
    // WHY: The second-pass native package contract should keep decoder-facing invariants separate from generic package metadata and reject underspecified packages before model loading.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString packageRoot = tempDir.filePath(QStringLiteral("native-decoder-package"));
    QVERIFY(writePackage(PackageFixtureRequest{
        .packageRoot = packageRoot,
        .weightsPayload = QStringLiteral("fixture native weights"),
        .nativeDecoder = true,
    }));

    QFile manifestFile(QDir(packageRoot).filePath(QStringLiteral("model.json")));
    QVERIFY(manifestFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll());
    manifestFile.close();
    QJsonObject root = document.object();
    root.remove(QStringLiteral("native_execution"));
    QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    manifestFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    manifestFile.close();

    RuntimeError error;
    const std::optional<ValidatedModelPackage> package =
        ModelValidator::validatePackagePath(packageRoot, cpuReferenceEngineName(), cpuReferenceModelFormat(), &error);

    QVERIFY(!package.has_value());
    QCOMPARE(error.code, RuntimeErrorCode::InvalidModelPackage);
}

void ModelPackageTest::validatorRejectsNativeDecoderPackageWithoutTokenizerAsset()
{
    // WHAT: Verify that mkasr-v2 native decoder packages must carry the tokenizer asset declared by their execution metadata.
    // HOW: Create a temporary native-decoder package, then remove the tokenizer asset entry before validation.
    // WHY: The future tensor-backed runtime needs package-owned tokenizer assets, so the validator should reject packages that only declare the contract without shipping the asset.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString packageRoot = tempDir.filePath(QStringLiteral("native-decoder-package"));
    QVERIFY(writePackage(PackageFixtureRequest{
        .packageRoot = packageRoot,
        .weightsPayload = QStringLiteral("fixture native weights"),
        .nativeDecoder = true,
    }));

    QFile manifestFile(QDir(packageRoot).filePath(QStringLiteral("model.json")));
    QVERIFY(manifestFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll());
    manifestFile.close();
    QJsonObject root = document.object();
    QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    assets.removeAt(1);
    root.insert(QStringLiteral("assets"), assets);
    QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    manifestFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    manifestFile.close();

    RuntimeError error;
    const std::optional<ValidatedModelPackage> package =
        ModelValidator::validatePackagePath(packageRoot, cpuReferenceEngineName(), cpuReferenceModelFormat(), &error);

    QVERIFY(!package.has_value());
    QCOMPARE(error.code, RuntimeErrorCode::InvalidModelPackage);
}

void ModelPackageTest::validatorRejectsBaselineDecoderPackageWithoutTokenizerMerges()
{
    // WHAT: Verify that execution-version-2 native packages must ship tokenizer merge rules.
    // HOW: Create a temporary native package with the baseline tokenizer contract, then remove the merge asset entry before validation.
    // WHY: The real native decoder lane needs a package-owned tokenizer model rather than the old whitespace-only fallback.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString packageRoot = tempDir.filePath(QStringLiteral("native-baseline-package"));
    QVERIFY(writePackage(PackageFixtureRequest{
        .packageRoot = packageRoot,
        .weightsPayload = QStringLiteral("fixture native weights"),
        .nativeDecoder = true,
        .baselineTokenizer = true,
    }));

    QFile manifestFile(QDir(packageRoot).filePath(QStringLiteral("model.json")));
    QVERIFY(manifestFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll());
    manifestFile.close();
    QJsonObject root = document.object();
    QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    assets.removeAt(2);
    root.insert(QStringLiteral("assets"), assets);
    QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    manifestFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    manifestFile.close();

    RuntimeError error;
    const std::optional<ValidatedModelPackage> package =
        ModelValidator::validatePackagePath(packageRoot, cpuReferenceEngineName(), cpuReferenceModelFormat(), &error);

    QVERIFY(!package.has_value());
    QCOMPARE(error.code, RuntimeErrorCode::InvalidModelPackage);
}

void ModelPackageTest::catalogSupportsLegacyRawWhisperCompatibility()
{
    // WHAT: Verify that the catalog can inspect a legacy raw Whisper model file through the compatibility path.
    // HOW: Write a minimal raw whisper.cpp ggml header fixture and inspect it through the model catalog.
    // WHY: Phase 4 keeps raw Whisper files supported during migration, but that support should still flow
    // through product-owned inspection rather than backend-specific loader behavior.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString rawPath = tempDir.filePath(QStringLiteral("ggml-base.en.bin"));
    QVERIFY(writeRawWhisperFixture(rawPath));

    RuntimeError error;
    const std::optional<ValidatedModelPackage> package = ModelCatalog::inspectPath(rawPath, {}, {}, &error);

    if (!package.has_value()) {
        QFAIL("Expected legacy compatibility package");
        return;
    }
    QVERIFY(error.isOk());
    const ValidatedModelPackage &legacyPackage = *package;
    QVERIFY(legacyPackage.isLegacyCompatibility());
    QCOMPARE(legacyPackage.metadata().sourceFormat, QStringLiteral("whisper.cpp-ggml"));
    QCOMPARE(legacyPackage.metadata().modelFormat, QStringLiteral("ggml"));
}

void ModelPackageTest::importerCreatesNativePackageFromRawWhisperFile()
{
    // WHAT: Verify that importing a legacy raw Whisper file produces a validated native package.
    // HOW: Create a minimal raw fixture, import it into a temporary models directory, and then
    // validate the resulting package through the normal package validator.
    // WHY: The native package format only becomes practical if there is a deterministic migration
    // path from the raw whisper.cpp files users already have on disk.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString rawPath = tempDir.filePath(QStringLiteral("ggml-base.en.bin"));
    QVERIFY(writeRawWhisperFixture(rawPath));

    RuntimeError error;
    const std::optional<ValidatedModelPackage> imported =
        RawWhisperImporter::importFile(rawPath,
                                       RawWhisperImportRequest{
                                           .outputPath = tempDir.filePath(QStringLiteral("base-en")),
                                       },
                                       &error);

    if (!imported.has_value()) {
        QFAIL("Expected imported native package");
        return;
    }
    QVERIFY(error.isOk());
    const ValidatedModelPackage &importedPackage = *imported;
    QVERIFY(!importedPackage.isLegacyCompatibility());
    QVERIFY(QFileInfo::exists(importedPackage.manifestPath));
    QVERIFY(QFileInfo::exists(importedPackage.weightsPath));
}

void ModelPackageTest::nativeExecutionJsonRoundTripsInitialPromptTokens()
{
    // WHAT: Verify that the native-execution JSON encoder and decoder round-trip the packaged initial prompt sequence.
    // HOW: Encode a manifest with three prompt tokens, decode it back, and assert the vector survives; then confirm an empty list omits the key and decodes to empty.
    // WHY: Phase 5B hoists Whisper prompt tokens into the package manifest so the runtime no longer hardcodes language/transcribe/no_timestamps ids.
    ModelPackageManifest manifest;
    manifest.format = QStringLiteral("mutterkey.model-package");
    manifest.schemaVersion = 1;
    manifest.nativeExecution.executionVersion = 2;
    manifest.nativeExecution.baselineFamily = QStringLiteral("whisper-base-en");
    manifest.nativeExecution.decoder = QStringLiteral("real-decoder-v3");
    manifest.nativeExecution.tokenizer = QStringLiteral("whisper-bpe");
    manifest.nativeExecution.tokenizerAssetRole = QStringLiteral("tokenizer_vocab");
    manifest.nativeExecution.tokenizerMergesAssetRole = QStringLiteral("tokenizer_merges");
    manifest.nativeExecution.frontend = QStringLiteral("log-mel-v1");
    manifest.nativeExecution.searchPolicy = QStringLiteral("greedy-real-v1");
    manifest.nativeExecution.timestampMode = QStringLiteral("timestamp-token-v1");
    manifest.nativeExecution.bosTokenId = 50257;
    manifest.nativeExecution.eosTokenId = 50256;
    manifest.nativeExecution.noSpeechTokenId = 50361;
    manifest.nativeExecution.timestampTokenStartId = 50363;
    manifest.nativeExecution.timestampTokenEndId = 51863;
    manifest.nativeExecution.initialPromptTokenIds = {50258, 50358, 50362};
    manifest.nativeExecution.suppressedTokenIds = {1, 2, 7, 8, 50257};

    const QJsonObject encoded = modelPackageManifestToJson(manifest);
    QString parseError;
    const std::optional<ModelPackageManifest> decoded = modelPackageManifestFromJson(encoded, &parseError);
    if (!decoded.has_value()) {
        QFAIL(qPrintable(parseError));
        return;
    }
    const ModelPackageManifest &decodedManifest = decoded.value();
    QCOMPARE(decodedManifest.nativeExecution.initialPromptTokenIds.size(), std::size_t{3});
    QCOMPARE(decodedManifest.nativeExecution.initialPromptTokenIds.at(0), 50258);
    QCOMPARE(decodedManifest.nativeExecution.initialPromptTokenIds.at(1), 50358);
    QCOMPARE(decodedManifest.nativeExecution.initialPromptTokenIds.at(2), 50362);
    QCOMPARE(decodedManifest.nativeExecution.suppressedTokenIds.size(), std::size_t{5});
    QCOMPARE(decodedManifest.nativeExecution.suppressedTokenIds.at(2), 7);

    // An empty prompt list must round-trip as an empty vector without a key,
    // so older manifests without the field keep working.
    ModelPackageManifest legacy = manifest;
    legacy.nativeExecution.initialPromptTokenIds.clear();
    legacy.nativeExecution.suppressedTokenIds.clear();
    const QJsonObject legacyEncoded = modelPackageManifestToJson(legacy);
    QVERIFY(!legacyEncoded.value(QStringLiteral("native_execution"))
                .toObject()
                .contains(QStringLiteral("initial_prompt_token_ids")));
    QVERIFY(!legacyEncoded.value(QStringLiteral("native_execution"))
                .toObject()
                .contains(QStringLiteral("suppressed_token_ids")));
    const std::optional<ModelPackageManifest> legacyDecoded = modelPackageManifestFromJson(legacyEncoded, &parseError);
    if (!legacyDecoded.has_value()) {
        QFAIL(qPrintable(parseError));
        return;
    }
    const ModelPackageManifest &legacyDecodedManifest = legacyDecoded.value();
    QVERIFY(legacyDecodedManifest.nativeExecution.initialPromptTokenIds.empty());
    QVERIFY(legacyDecodedManifest.nativeExecution.suppressedTokenIds.empty());
}

QTEST_APPLESS_MAIN(ModelPackageTest)

#include "modelpackagetest.moc"
