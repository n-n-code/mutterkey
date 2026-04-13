#include "asr/model/modelcatalog.h"

#include "asr/model/modelvalidator.h"
#include "asr/model/rawwhisperprobe.h"

#include <QFileInfo>

namespace {

RuntimeError makeRuntimeError(RuntimeErrorCode code, QString message, QString detail = {})
{
    return RuntimeError{.code = code, .message = std::move(message), .detail = std::move(detail)};
}

ModelPackageManifest legacyManifestForPath(const QString &sourcePath, const ModelMetadata &metadata)
{
    ModelPackageManifest manifest;
    manifest.format = QStringLiteral("mutterkey.model-package");
    manifest.schemaVersion = 1;
    manifest.metadata = metadata;
    manifest.compatibleEngines.push_back(ModelCompatibilityMarker{
        .engine = legacyWhisperEngineName(),
        .modelFormat = legacyWhisperModelFormat(),
    });
    manifest.assets.push_back(ModelAssetMetadata{
        .role = QStringLiteral("weights"),
        .relativePath = QFileInfo(sourcePath).fileName(),
        .sha256 = {},
        .sizeBytes = QFileInfo(sourcePath).size(),
    });
    manifest.sourceArtifact = sourcePath;
    return manifest;
}

} // namespace

std::optional<ValidatedModelPackage> ModelCatalog::inspectPath(const QString &path,
                                                               QStringView requiredEngine,
                                                               QStringView requiredModelFormat,
                                                               RuntimeError *error)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelNotFound,
                                      QStringLiteral("Model artifact not found: %1").arg(path),
                                      info.absoluteFilePath());
        }
        return std::nullopt;
    }

    if (info.isDir() || info.fileName() == QStringLiteral("model.json")) {
        return ModelValidator::validatePackagePath(path, requiredEngine, requiredModelFormat, error);
    }

    RuntimeError probeError;
    const std::optional<ModelMetadata> metadata = RawWhisperProbe::inspectFile(path, &probeError);
    if (!metadata.has_value()) {
        if (error != nullptr) {
            *error = probeError;
        }
        return std::nullopt;
    }

    if ((!requiredEngine.isEmpty() && requiredEngine != legacyWhisperEngineName())
        || (!requiredModelFormat.isEmpty() && requiredModelFormat != legacyWhisperModelFormat())) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::IncompatibleModelPackage,
                                      QStringLiteral("Raw Whisper compatibility artifact does not match the active runtime"));
        }
        return std::nullopt;
    }

    return ValidatedModelPackage{
        .packageRootPath = QFileInfo(path).absolutePath(),
        .manifestPath = {},
        .sourcePath = QFileInfo(path).absoluteFilePath(),
        .weightsPath = QFileInfo(path).absoluteFilePath(),
        .manifest = legacyManifestForPath(QFileInfo(path).absoluteFilePath(), *metadata),
    };
}
