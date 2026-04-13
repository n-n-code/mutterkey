#include "asr/model/modelvalidator.h"

#include <algorithm>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

namespace {

RuntimeError makeRuntimeError(RuntimeErrorCode code, QString message, QString detail = {})
{
    return RuntimeError{.code = code, .message = std::move(message), .detail = std::move(detail)};
}

RuntimeError invalidPackage(QString message, QString detail = {})
{
    return makeRuntimeError(RuntimeErrorCode::InvalidModelPackage, std::move(message), std::move(detail));
}

RuntimeError incompatiblePackage(QString message, QString detail = {})
{
    return makeRuntimeError(RuntimeErrorCode::IncompatibleModelPackage, std::move(message), std::move(detail));
}

RuntimeError integrityFailure(QString message, QString detail = {})
{
    return makeRuntimeError(RuntimeErrorCode::ModelIntegrityFailed, std::move(message), std::move(detail));
}

RuntimeError modelTooLarge(QString message, QString detail = {})
{
    return makeRuntimeError(RuntimeErrorCode::ModelTooLarge, std::move(message), std::move(detail));
}

bool isRelativeSafePath(const QString &path);

bool computeSha256(const QString &path, QString *digest, RuntimeError *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = integrityFailure(QStringLiteral("Failed to open model asset for hashing"),
                                      QFileInfo(path).absoluteFilePath());
        }
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        if (error != nullptr) {
            *error = integrityFailure(QStringLiteral("Failed to hash model asset"), QFileInfo(path).absoluteFilePath());
        }
        return false;
    }

    if (digest != nullptr) {
        *digest = QString::fromLatin1(hash.result().toHex());
    }
    return true;
}

bool validateManifestAsset(const QString &packageRootPath,
                          const ModelAssetMetadata &asset,
                          const ModelValidationLimits &limits,
                          RuntimeError *error)
{
    if (!isRelativeSafePath(asset.relativePath)) {
        if (error != nullptr) {
            *error = integrityFailure(QStringLiteral("Model asset path is not safe"), asset.relativePath);
        }
        return false;
    }
    if (asset.sizeBytes < 0 || asset.sizeBytes > limits.maxWeightsBytes) {
        if (error != nullptr) {
            *error = modelTooLarge(QStringLiteral("Model asset exceeds supported size limits"), asset.relativePath);
        }
        return false;
    }

    const QString assetPath = QDir(packageRootPath).filePath(QDir::cleanPath(asset.relativePath));
    const QFileInfo assetInfo(assetPath);
    if (!assetInfo.exists() || !assetInfo.isFile()) {
        if (error != nullptr) {
            *error = integrityFailure(QStringLiteral("Model asset is missing"), assetPath);
        }
        return false;
    }
    if (assetInfo.isSymLink()) {
        if (error != nullptr) {
            *error = integrityFailure(QStringLiteral("Model asset must not be a symlink"), assetPath);
        }
        return false;
    }
    if (assetInfo.size() != asset.sizeBytes) {
        if (error != nullptr) {
            *error = integrityFailure(QStringLiteral("Model asset size does not match manifest"), assetPath);
        }
        return false;
    }

    QString hashDigest;
    if (!computeSha256(assetPath, &hashDigest, error)) {
        return false;
    }
    if (!asset.sha256.isEmpty() && hashDigest != asset.sha256.toLower()) {
        if (error != nullptr) {
            *error = integrityFailure(QStringLiteral("Model asset hash does not match manifest"), assetPath);
        }
        return false;
    }

    return true;
}

bool isRelativeSafePath(const QString &path)
{
    if (path.isEmpty() || QDir::isAbsolutePath(path) || path.contains(QStringLiteral(".."))) {
        return false;
    }

    const QString cleaned = QDir::cleanPath(path);
    return !cleaned.startsWith(QStringLiteral("../")) && cleaned != QStringLiteral("..");
}

bool hasRequiredCompatibility(const ModelPackageManifest &manifest, QStringView engine, QStringView modelFormat)
{
    if (engine.isEmpty() && modelFormat.isEmpty()) {
        return true;
    }

    if (!engine.isEmpty() && !modelFormat.isEmpty()) {
        return modelPackageSupportsCompatibility(manifest, engine, modelFormat);
    }

    return std::ranges::any_of(manifest.compatibleEngines, [engine, modelFormat](const ModelCompatibilityMarker &marker) {
        const bool engineMatches = engine.isEmpty() || marker.engine == engine;
        const bool formatMatches = modelFormat.isEmpty() || marker.modelFormat == modelFormat;
        return engineMatches && formatMatches;
    });
}

bool requiresNativeExecutionMetadata(const ModelPackageManifest &manifest)
{
    return modelPackageSupportsCompatibility(manifest, cpuReferenceEngineName(), cpuReferenceModelFormat());
}

bool hasValidNativeExecutionMetadata(const NativeExecutionMetadata &metadata)
{
    if (metadata.executionVersion == 1) {
        return !metadata.decoder.isEmpty() && !metadata.tokenizer.isEmpty() && !metadata.tokenizerAssetRole.isEmpty()
            && !metadata.frontend.isEmpty() && !metadata.searchPolicy.isEmpty() && !metadata.timestampMode.isEmpty()
            && metadata.featureBinCount > 0 && metadata.templateCount > 0 && metadata.maxDistance > 0.0
            && metadata.bosTokenId >= 0 && metadata.eosTokenId >= 0 && metadata.noSpeechTokenId >= 0
            && metadata.timestampTokenStartId >= 0 && metadata.timestampTokenEndId >= metadata.timestampTokenStartId;
    }

    if (metadata.executionVersion >= 2) {
        return !metadata.baselineFamily.isEmpty() && !metadata.decoder.isEmpty() && !metadata.tokenizer.isEmpty()
            && !metadata.tokenizerAssetRole.isEmpty() && !metadata.tokenizerMergesAssetRole.isEmpty()
            && !metadata.frontend.isEmpty() && !metadata.searchPolicy.isEmpty() && !metadata.timestampMode.isEmpty()
            && metadata.bosTokenId >= 0 && metadata.eosTokenId >= 0 && metadata.noSpeechTokenId >= 0
            && metadata.timestampTokenStartId >= 0 && metadata.timestampTokenEndId >= metadata.timestampTokenStartId;
    }

    return false;
}

bool hasSaneNativeDecoderMetadata(const ModelPackageManifest &manifest)
{
    const NativeExecutionMetadata &metadata = manifest.nativeExecution;
    if (metadata.executionVersion == 1) {
        return manifest.metadata.vocabularySize > metadata.timestampTokenEndId && manifest.metadata.vocabularySize > 0
            && manifest.metadata.melCount > 0;
    }

    if (metadata.executionVersion >= 2) {
        return manifest.metadata.vocabularySize > metadata.timestampTokenEndId && manifest.metadata.vocabularySize > 0
            && manifest.metadata.melCount > 0 && !manifest.metadata.architecture.isEmpty();
    }

    return false;
}

} // namespace

ModelValidationLimits ModelValidator::defaultLimits()
{
    return ModelValidationLimits{};
}

std::optional<ValidatedModelPackage> ModelValidator::validatePackagePath(const QString &path,
                                                                         QStringView requiredEngine,
                                                                         QStringView requiredModelFormat,
                                                                         RuntimeError *error,
                                                                         const ModelValidationLimits &limits)
{
    const QFileInfo inputInfo(path);
    if (!inputInfo.exists()) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelNotFound,
                                      QStringLiteral("Model package not found: %1").arg(path),
                                      inputInfo.absoluteFilePath());
        }
        return std::nullopt;
    }

    QString manifestPath;
    QString packageRootPath;
    if (inputInfo.isDir()) {
        packageRootPath = inputInfo.absoluteFilePath();
        manifestPath = QDir(packageRootPath).filePath(QStringLiteral("model.json"));
    } else {
        manifestPath = inputInfo.absoluteFilePath();
        packageRootPath = QFileInfo(manifestPath).absolutePath();
    }

    const QFileInfo manifestInfo(manifestPath);
    if (!manifestInfo.exists() || !manifestInfo.isFile()) {
        if (error != nullptr) {
            *error = invalidPackage(QStringLiteral("Model package manifest not found: %1").arg(manifestPath));
        }
        return std::nullopt;
    }
    if (manifestInfo.isSymLink()) {
        if (error != nullptr) {
            *error = integrityFailure(QStringLiteral("Model package manifest must not be a symlink"), manifestPath);
        }
        return std::nullopt;
    }
    if (manifestInfo.size() > limits.maxManifestBytes) {
        if (error != nullptr) {
            *error = modelTooLarge(QStringLiteral("Model package manifest is too large"), manifestPath);
        }
        return std::nullopt;
    }

    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error != nullptr) {
            *error = invalidPackage(QStringLiteral("Failed to open model package manifest"), manifestPath);
        }
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error != nullptr) {
            *error = invalidPackage(QStringLiteral("Invalid model package manifest"),
                                    parseError.errorString());
        }
        return std::nullopt;
    }

    QString manifestError;
    const std::optional<ModelPackageManifest> manifest = modelPackageManifestFromJson(document.object(), &manifestError);
    if (!manifest.has_value()) {
        if (error != nullptr) {
            *error = invalidPackage(QStringLiteral("Invalid model package manifest"), manifestError);
        }
        return std::nullopt;
    }

    if (manifest->format != QStringLiteral("mutterkey.model-package")) {
        if (error != nullptr) {
            *error = invalidPackage(QStringLiteral("Unsupported model package format"), manifest->format);
        }
        return std::nullopt;
    }
    if (manifest->schemaVersion != 1) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::UnsupportedModelPackageVersion,
                                      QStringLiteral("Unsupported model package schema version: %1").arg(manifest->schemaVersion));
        }
        return std::nullopt;
    }
    if (manifest->assets.empty() || manifest->assets.size() > static_cast<std::size_t>(limits.maxAssetCount)) {
        if (error != nullptr) {
            *error = invalidPackage(QStringLiteral("Model package asset count is invalid"));
        }
        return std::nullopt;
    }
    if (manifest->metadata.packageId.isEmpty() || manifest->metadata.runtimeFamily != QStringLiteral("asr")) {
        if (error != nullptr) {
            *error = invalidPackage(QStringLiteral("Model package metadata is incomplete"));
        }
        return std::nullopt;
    }
    if (!hasRequiredCompatibility(*manifest, requiredEngine, requiredModelFormat)) {
        if (error != nullptr) {
            *error = incompatiblePackage(QStringLiteral("Model package is not compatible with the active runtime"),
                                         QStringLiteral("%1 / %2").arg(requiredEngine, requiredModelFormat));
        }
        return std::nullopt;
    }
    if (requiresNativeExecutionMetadata(*manifest) && !hasValidNativeExecutionMetadata(manifest->nativeExecution)) {
        if (error != nullptr) {
            *error = invalidPackage(QStringLiteral("Native CPU decoder package is missing execution metadata"));
        }
        return std::nullopt;
    }
    if (requiresNativeExecutionMetadata(*manifest) && !hasSaneNativeDecoderMetadata(*manifest)) {
        if (error != nullptr) {
            *error = invalidPackage(QStringLiteral("Native CPU decoder package metadata is internally inconsistent"));
        }
        return std::nullopt;
    }

    const std::optional<ModelAssetMetadata> asset = modelPackageAssetByRole(*manifest, QStringLiteral("weights"));
    if (!asset.has_value()) {
        if (error != nullptr) {
            *error = invalidPackage(QStringLiteral("Model package is missing a weights asset"));
        }
        return std::nullopt;
    }
    const QString weightsPath = QDir(packageRootPath).filePath(QDir::cleanPath(asset->relativePath));
    if (!validateManifestAsset(packageRootPath, *asset, limits, error)) {
        return std::nullopt;
    }

    qint64 packageBytes = manifestInfo.size();
    for (const ModelAssetMetadata &manifestAsset : manifest->assets) {
        packageBytes += manifestAsset.sizeBytes;
    }
    if (packageBytes > limits.maxPackageBytes) {
        if (error != nullptr) {
            *error = modelTooLarge(QStringLiteral("Model package exceeds supported size limits"), packageRootPath);
        }
        return std::nullopt;
    }

    if (requiresNativeExecutionMetadata(*manifest)) {
        const std::optional<ModelAssetMetadata> tokenizerAsset =
            modelPackageAssetByRole(*manifest, manifest->nativeExecution.tokenizerAssetRole);
        if (!tokenizerAsset.has_value()) {
            if (error != nullptr) {
                *error = invalidPackage(QStringLiteral("Native CPU decoder package is missing a tokenizer asset"));
            }
            return std::nullopt;
        }
        if (!validateManifestAsset(packageRootPath, *tokenizerAsset, limits, error)) {
            return std::nullopt;
        }

        if (manifest->nativeExecution.executionVersion >= 2) {
            const std::optional<ModelAssetMetadata> mergesAsset =
                modelPackageAssetByRole(*manifest, manifest->nativeExecution.tokenizerMergesAssetRole);
            if (!mergesAsset.has_value()) {
                if (error != nullptr) {
                    *error = invalidPackage(QStringLiteral("Native CPU decoder package is missing tokenizer merge assets"));
                }
                return std::nullopt;
            }
            if (!validateManifestAsset(packageRootPath, *mergesAsset, limits, error)) {
                return std::nullopt;
            }
        }
    }

    return ValidatedModelPackage{
        .packageRootPath = packageRootPath,
        .manifestPath = manifestPath,
        .sourcePath = QFileInfo(path).absoluteFilePath(),
        .weightsPath = weightsPath,
        .manifest = *manifest,
    };
}
