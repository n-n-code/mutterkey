#include "transcription/modelvalidator.h"

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

bool isRelativeSafePath(const QString &path)
{
    if (path.isEmpty() || QDir::isAbsolutePath(path) || path.contains(QStringLiteral(".."))) {
        return false;
    }

    const QString cleaned = QDir::cleanPath(path);
    return !cleaned.startsWith(QStringLiteral("../")) && cleaned != QStringLiteral("..");
}

std::optional<ModelAssetMetadata> weightsAsset(const ModelPackageManifest &manifest)
{
    for (const ModelAssetMetadata &asset : manifest.assets) {
        if (asset.role == QStringLiteral("weights")) {
            return asset;
        }
    }
    return std::nullopt;
}

bool hasRequiredCompatibility(const ModelPackageManifest &manifest, QStringView engine, QStringView modelFormat)
{
    if (engine.isEmpty() && modelFormat.isEmpty()) {
        return true;
    }

    return std::ranges::any_of(manifest.compatibleEngines, [engine, modelFormat](const ModelCompatibilityMarker &marker) {
        const bool engineMatches = engine.isEmpty() || marker.engine == engine;
        const bool formatMatches = modelFormat.isEmpty() || marker.modelFormat == modelFormat;
        return engineMatches && formatMatches;
    });
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

    const std::optional<ModelAssetMetadata> asset = weightsAsset(*manifest);
    if (!asset.has_value()) {
        if (error != nullptr) {
            *error = invalidPackage(QStringLiteral("Model package is missing a weights asset"));
        }
        return std::nullopt;
    }
    if (!isRelativeSafePath(asset->relativePath)) {
        if (error != nullptr) {
            *error = integrityFailure(QStringLiteral("Model asset path is not safe"), asset->relativePath);
        }
        return std::nullopt;
    }
    if (asset->sizeBytes < 0 || asset->sizeBytes > limits.maxWeightsBytes) {
        if (error != nullptr) {
            *error = modelTooLarge(QStringLiteral("Model weights asset exceeds supported size limits"));
        }
        return std::nullopt;
    }

    const QString weightsPath = QDir(packageRootPath).filePath(QDir::cleanPath(asset->relativePath));
    const QFileInfo weightsInfo(weightsPath);
    if (!weightsInfo.exists() || !weightsInfo.isFile()) {
        if (error != nullptr) {
            *error = integrityFailure(QStringLiteral("Model weights asset is missing"), weightsPath);
        }
        return std::nullopt;
    }
    if (weightsInfo.isSymLink()) {
        if (error != nullptr) {
            *error = integrityFailure(QStringLiteral("Model weights asset must not be a symlink"), weightsPath);
        }
        return std::nullopt;
    }
    if (weightsInfo.size() != asset->sizeBytes) {
        if (error != nullptr) {
            *error = integrityFailure(QStringLiteral("Model weights asset size does not match manifest"), weightsPath);
        }
        return std::nullopt;
    }

    const qint64 packageBytes = manifestInfo.size() + weightsInfo.size();
    if (packageBytes > limits.maxPackageBytes) {
        if (error != nullptr) {
            *error = modelTooLarge(QStringLiteral("Model package exceeds supported size limits"), packageRootPath);
        }
        return std::nullopt;
    }

    QString hashDigest;
    if (!computeSha256(weightsPath, &hashDigest, error)) {
        return std::nullopt;
    }
    if (!asset->sha256.isEmpty() && hashDigest != asset->sha256.toLower()) {
        if (error != nullptr) {
            *error = integrityFailure(QStringLiteral("Model weights hash does not match manifest"), weightsPath);
        }
        return std::nullopt;
    }

    return ValidatedModelPackage{
        .packageRootPath = packageRootPath,
        .manifestPath = manifestPath,
        .sourcePath = QFileInfo(path).absoluteFilePath(),
        .weightsPath = weightsPath,
        .manifest = *manifest,
    };
}
