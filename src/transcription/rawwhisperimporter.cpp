#include "transcription/rawwhisperimporter.h"

#include "transcription/modelvalidator.h"
#include "transcription/rawwhisperprobe.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace {

RuntimeError makeRuntimeError(RuntimeErrorCode code, QString message, QString detail = {})
{
    return RuntimeError{.code = code, .message = std::move(message), .detail = std::move(detail)};
}

bool writeFileHash(const QString &path, QString *digest, RuntimeError *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelNotFound,
                                      QStringLiteral("Failed to open raw Whisper model for import"),
                                      QFileInfo(path).absoluteFilePath());
        }
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelIntegrityFailed,
                                      QStringLiteral("Failed to hash raw Whisper model during import"));
        }
        return false;
    }

    if (digest != nullptr) {
        *digest = QString::fromLatin1(hash.result().toHex());
    }
    return true;
}

QString resolvePackageDirectory(const RawWhisperImportRequest &request, const QString &packageId)
{
    const QString &outputPath = request.outputPath;
    if (outputPath.isEmpty()) {
        return QDir(defaultModelPackageDirectory()).filePath(packageId);
    }

    const QFileInfo outputInfo(outputPath);
    if (!outputInfo.exists() || outputInfo.isDir()) {
        if (outputPath.endsWith(QLatin1String(".json"))) {
            return QFileInfo(outputPath).absolutePath();
        }
        return outputInfo.absoluteFilePath();
    }

    return QFileInfo(outputPath).absolutePath();
}

} // namespace

std::optional<ValidatedModelPackage> RawWhisperImporter::importFile(const QString &sourcePath,
                                                                    const RawWhisperImportRequest &request,
                                                                    RuntimeError *error)
{
    RuntimeError probeError;
    std::optional<ModelMetadata> metadata = RawWhisperProbe::inspectFile(sourcePath, &probeError);
    if (!metadata.has_value()) {
        if (error != nullptr) {
            *error = probeError;
        }
        return std::nullopt;
    }

    const QString packageId =
        sanitizePackageId(request.packageIdOverride.isEmpty() ? metadata->packageId : request.packageIdOverride);
    if (packageId.isEmpty()) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::InvalidModelPackage,
                                      QStringLiteral("Imported model package id would be empty"));
        }
        return std::nullopt;
    }

    metadata->packageId = packageId;
    metadata->legacyCompatibility = false;

    const QString packageDirectory = resolvePackageDirectory(request, packageId);
    if (QFileInfo::exists(packageDirectory)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::InvalidConfig,
                                      QStringLiteral("Refusing to overwrite an existing model package"),
                                      packageDirectory);
        }
        return std::nullopt;
    }

    const QDir root;
    if (!root.mkpath(QDir(packageDirectory).filePath(QStringLiteral("assets")))) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::InvalidConfig,
                                      QStringLiteral("Failed to create model package directory"),
                                      packageDirectory);
        }
        return std::nullopt;
    }

    const QString weightsPath = QDir(packageDirectory).filePath(QStringLiteral("assets/model.bin"));
    if (!QFile::copy(sourcePath, weightsPath)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelIntegrityFailed,
                                      QStringLiteral("Failed to copy raw Whisper model into package"),
                                      weightsPath);
        }
        return std::nullopt;
    }

    QString weightsHash;
    if (!writeFileHash(weightsPath, &weightsHash, error)) {
        return std::nullopt;
    }

    ModelPackageManifest manifest;
    manifest.format = QStringLiteral("mutterkey.model-package");
    manifest.schemaVersion = 1;
    manifest.metadata = *metadata;
    manifest.compatibleEngines.push_back(ModelCompatibilityMarker{
        .engine = legacyWhisperEngineName(),
        .modelFormat = legacyWhisperModelFormat(),
    });
    manifest.assets.push_back(ModelAssetMetadata{
        .role = QStringLiteral("weights"),
        .relativePath = QStringLiteral("assets/model.bin"),
        .sha256 = weightsHash,
        .sizeBytes = QFileInfo(weightsPath).size(),
    });
    manifest.sourceArtifact = QFileInfo(sourcePath).absoluteFilePath();

    const QString manifestPath = QDir(packageDirectory).filePath(QStringLiteral("model.json"));
    QSaveFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::InvalidConfig,
                                      QStringLiteral("Failed to create model package manifest"),
                                      manifestPath);
        }
        return std::nullopt;
    }

    const QJsonDocument document(modelPackageManifestToJson(manifest));
    manifestFile.write(document.toJson(QJsonDocument::Indented));
    if (!manifestFile.commit()) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::InvalidConfig,
                                      QStringLiteral("Failed to save model package manifest"),
                                      manifestPath);
        }
        return std::nullopt;
    }

    return ModelValidator::validatePackagePath(packageDirectory, legacyWhisperEngineName(), legacyWhisperModelFormat(), error);
}
