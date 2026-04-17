#include "asr/model/modelpackage.h"

#include <algorithm>
#include <QDir>
#include <QJsonArray>
#include <QStandardPaths>

namespace {

QString readString(const QJsonObject &object, QStringView key)
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString().trimmed() : QString{};
}

int readInt(const QJsonObject &object, QStringView key)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toInt() : 0;
}

double readDouble(const QJsonObject &object, QStringView key)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toDouble() : 0.0;
}

void setIfNotEmpty(QJsonObject *object, QStringView key, const QString &value)
{
    if (object != nullptr && !value.isEmpty()) {
        object->insert(key.toString(), value);
    }
}

QJsonObject metadataToJson(const ModelMetadata &metadata)
{
    QJsonObject object;
    setIfNotEmpty(&object, QStringLiteral("package_id"), metadata.packageId);
    setIfNotEmpty(&object, QStringLiteral("display_name"), metadata.displayName);
    setIfNotEmpty(&object, QStringLiteral("package_version"), metadata.packageVersion);
    setIfNotEmpty(&object, QStringLiteral("runtime_family"), metadata.runtimeFamily);
    setIfNotEmpty(&object, QStringLiteral("source_format"), metadata.sourceFormat);
    setIfNotEmpty(&object, QStringLiteral("model_format"), metadata.modelFormat);
    setIfNotEmpty(&object, QStringLiteral("architecture"), metadata.architecture);
    setIfNotEmpty(&object, QStringLiteral("language_profile"), metadata.languageProfile);
    setIfNotEmpty(&object, QStringLiteral("quantization"), metadata.quantization);
    setIfNotEmpty(&object, QStringLiteral("tokenizer"), metadata.tokenizer);
    object.insert(QStringLiteral("legacy_compatibility"), metadata.legacyCompatibility);
    object.insert(QStringLiteral("vocabulary_size"), metadata.vocabularySize);
    object.insert(QStringLiteral("audio_context"), metadata.audioContext);
    object.insert(QStringLiteral("audio_state"), metadata.audioState);
    object.insert(QStringLiteral("audio_head_count"), metadata.audioHeadCount);
    object.insert(QStringLiteral("audio_layer_count"), metadata.audioLayerCount);
    object.insert(QStringLiteral("text_context"), metadata.textContext);
    object.insert(QStringLiteral("text_state"), metadata.textState);
    object.insert(QStringLiteral("text_head_count"), metadata.textHeadCount);
    object.insert(QStringLiteral("text_layer_count"), metadata.textLayerCount);
    object.insert(QStringLiteral("mel_count"), metadata.melCount);
    object.insert(QStringLiteral("format_type"), metadata.formatType);
    return object;
}

ModelMetadata metadataFromJson(const QJsonObject &object)
{
    return ModelMetadata{
        .packageId = readString(object, QStringLiteral("package_id")),
        .displayName = readString(object, QStringLiteral("display_name")),
        .packageVersion = readString(object, QStringLiteral("package_version")),
        .runtimeFamily = readString(object, QStringLiteral("runtime_family")),
        .sourceFormat = readString(object, QStringLiteral("source_format")),
        .modelFormat = readString(object, QStringLiteral("model_format")),
        .architecture = readString(object, QStringLiteral("architecture")),
        .languageProfile = readString(object, QStringLiteral("language_profile")),
        .quantization = readString(object, QStringLiteral("quantization")),
        .tokenizer = readString(object, QStringLiteral("tokenizer")),
        .legacyCompatibility = object.value(QStringLiteral("legacy_compatibility")).toBool(false),
        .vocabularySize = readInt(object, QStringLiteral("vocabulary_size")),
        .audioContext = readInt(object, QStringLiteral("audio_context")),
        .audioState = readInt(object, QStringLiteral("audio_state")),
        .audioHeadCount = readInt(object, QStringLiteral("audio_head_count")),
        .audioLayerCount = readInt(object, QStringLiteral("audio_layer_count")),
        .textContext = readInt(object, QStringLiteral("text_context")),
        .textState = readInt(object, QStringLiteral("text_state")),
        .textHeadCount = readInt(object, QStringLiteral("text_head_count")),
        .textLayerCount = readInt(object, QStringLiteral("text_layer_count")),
        .melCount = readInt(object, QStringLiteral("mel_count")),
        .formatType = readInt(object, QStringLiteral("format_type")),
    };
}

QJsonObject nativeExecutionToJson(const NativeExecutionMetadata &metadata)
{
    QJsonObject object;
    object.insert(QStringLiteral("execution_version"), metadata.executionVersion);
    setIfNotEmpty(&object, QStringLiteral("baseline_family"), metadata.baselineFamily);
    setIfNotEmpty(&object, QStringLiteral("decoder"), metadata.decoder);
    setIfNotEmpty(&object, QStringLiteral("tokenizer"), metadata.tokenizer);
    setIfNotEmpty(&object, QStringLiteral("tokenizer_asset_role"), metadata.tokenizerAssetRole);
    setIfNotEmpty(&object, QStringLiteral("tokenizer_merges_asset_role"), metadata.tokenizerMergesAssetRole);
    setIfNotEmpty(&object, QStringLiteral("frontend"), metadata.frontend);
    setIfNotEmpty(&object, QStringLiteral("search_policy"), metadata.searchPolicy);
    setIfNotEmpty(&object, QStringLiteral("timestamp_mode"), metadata.timestampMode);
    object.insert(QStringLiteral("feature_bin_count"), metadata.featureBinCount);
    object.insert(QStringLiteral("template_count"), metadata.templateCount);
    object.insert(QStringLiteral("max_distance"), metadata.maxDistance);
    object.insert(QStringLiteral("bos_token_id"), metadata.bosTokenId);
    object.insert(QStringLiteral("eos_token_id"), metadata.eosTokenId);
    object.insert(QStringLiteral("no_speech_token_id"), metadata.noSpeechTokenId);
    object.insert(QStringLiteral("timestamp_token_start_id"), metadata.timestampTokenStartId);
    object.insert(QStringLiteral("timestamp_token_end_id"), metadata.timestampTokenEndId);
    if (!metadata.initialPromptTokenIds.empty()) {
        QJsonArray promptArray;
        for (const int tokenId : metadata.initialPromptTokenIds) {
            promptArray.append(tokenId);
        }
        object.insert(QStringLiteral("initial_prompt_token_ids"), promptArray);
    }
    if (!metadata.suppressedTokenIds.empty()) {
        QJsonArray suppressedArray;
        for (const int tokenId : metadata.suppressedTokenIds) {
            suppressedArray.append(tokenId);
        }
        object.insert(QStringLiteral("suppressed_token_ids"), suppressedArray);
    }
    return object;
}

NativeExecutionMetadata nativeExecutionFromJson(const QJsonObject &object)
{
    NativeExecutionMetadata metadata{
        .executionVersion = readInt(object, QStringLiteral("execution_version")),
        .baselineFamily = readString(object, QStringLiteral("baseline_family")),
        .decoder = readString(object, QStringLiteral("decoder")),
        .tokenizer = readString(object, QStringLiteral("tokenizer")),
        .tokenizerAssetRole = readString(object, QStringLiteral("tokenizer_asset_role")),
        .tokenizerMergesAssetRole = readString(object, QStringLiteral("tokenizer_merges_asset_role")),
        .frontend = readString(object, QStringLiteral("frontend")),
        .searchPolicy = readString(object, QStringLiteral("search_policy")),
        .timestampMode = readString(object, QStringLiteral("timestamp_mode")),
        .featureBinCount = readInt(object, QStringLiteral("feature_bin_count")),
        .templateCount = readInt(object, QStringLiteral("template_count")),
        .maxDistance = readDouble(object, QStringLiteral("max_distance")),
        .bosTokenId = readInt(object, QStringLiteral("bos_token_id")),
        .eosTokenId = readInt(object, QStringLiteral("eos_token_id")),
        .noSpeechTokenId = readInt(object, QStringLiteral("no_speech_token_id")),
        .timestampTokenStartId = readInt(object, QStringLiteral("timestamp_token_start_id")),
        .timestampTokenEndId = readInt(object, QStringLiteral("timestamp_token_end_id")),
    };

    const QJsonArray promptArray = object.value(QStringLiteral("initial_prompt_token_ids")).toArray();
    metadata.initialPromptTokenIds.reserve(static_cast<std::size_t>(promptArray.size()));
    for (const auto &value : promptArray) {
        if (value.isDouble()) {
            metadata.initialPromptTokenIds.push_back(value.toInt());
        }
    }
    const QJsonArray suppressedArray = object.value(QStringLiteral("suppressed_token_ids")).toArray();
    metadata.suppressedTokenIds.reserve(static_cast<std::size_t>(suppressedArray.size()));
    for (const auto &value : suppressedArray) {
        if (value.isDouble()) {
            metadata.suppressedTokenIds.push_back(value.toInt());
        }
    }
    return metadata;
}

} // namespace

QString ValidatedModelPackage::description() const
{
    const QString packageLabel = manifest.metadata.displayName.isEmpty() ? manifest.metadata.packageId : manifest.metadata.displayName;
    QString sourceLabel = sourcePath.isEmpty() ? weightsPath : sourcePath;
    if (packageLabel.isEmpty()) {
        return sourceLabel;
    }
    return QStringLiteral("%1 (%2)").arg(packageLabel, sourceLabel);
}

std::optional<ModelAssetMetadata> ValidatedModelPackage::assetByRole(QStringView role) const
{
    return modelPackageAssetByRole(manifest, role);
}

std::optional<QString> ValidatedModelPackage::resolvedAssetPath(QStringView role) const
{
    const std::optional<ModelAssetMetadata> asset = assetByRole(role);
    if (!asset.has_value()) {
        return std::nullopt;
    }

    return QDir(packageRootPath).filePath(QDir::cleanPath(asset->relativePath));
}

QString defaultModelPackageDirectory()
{
    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dataRoot).filePath(QStringLiteral("models"));
}

QString sanitizePackageId(const QString &value)
{
    QString sanitized;
    sanitized.reserve(value.size());
    for (const QChar character : value.trimmed().toLower()) {
        if (character.isLetterOrNumber()) {
            sanitized.append(character);
            continue;
        }
        if (character == QLatin1Char('-') || character == QLatin1Char('_')) {
            sanitized.append(QLatin1Char('-'));
            continue;
        }
        if (character.isSpace() || character == QLatin1Char('.')) {
            sanitized.append(QLatin1Char('-'));
        }
    }

    while (sanitized.contains(QStringLiteral("--"))) {
        sanitized.replace(QStringLiteral("--"), QStringLiteral("-"));
    }
    sanitized = sanitized.trimmed();
    while (sanitized.startsWith(QLatin1Char('-'))) {
        sanitized.remove(0, 1);
    }
    while (sanitized.endsWith(QLatin1Char('-'))) {
        sanitized.chop(1);
    }
    return sanitized;
}

QString cpuReferenceEngineName()
{
    return QStringLiteral("mutterkey.cpu-reference");
}

QString cpuReferenceModelFormat()
{
    return QStringLiteral("mkasr-v2");
}

QString cpuReferenceFixtureModelFormat()
{
    return QStringLiteral("mkasr-v1");
}

QString legacyWhisperEngineName()
{
    return QStringLiteral("whisper.cpp");
}

QString legacyWhisperModelFormat()
{
    return QStringLiteral("ggml");
}

bool modelPackageSupportsCompatibility(const ModelPackageManifest &manifest, QStringView engine, QStringView modelFormat)
{
    return std::ranges::any_of(manifest.compatibleEngines, [engine, modelFormat](const ModelCompatibilityMarker &marker) {
        return marker.engine == engine && marker.modelFormat == modelFormat;
    });
}

std::optional<ModelAssetMetadata> modelPackageAssetByRole(const ModelPackageManifest &manifest, QStringView role)
{
    for (const ModelAssetMetadata &asset : manifest.assets) {
        if (asset.role == role) {
            return asset;
        }
    }

    return std::nullopt;
}

QJsonObject modelPackageManifestToJson(const ModelPackageManifest &manifest)
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), manifest.format);
    root.insert(QStringLiteral("schema_version"), manifest.schemaVersion);
    root.insert(QStringLiteral("metadata"), metadataToJson(manifest.metadata));
    if (manifest.nativeExecution.executionVersion > 0 || !manifest.nativeExecution.baselineFamily.isEmpty()
        || !manifest.nativeExecution.decoder.isEmpty()
        || !manifest.nativeExecution.tokenizer.isEmpty() || !manifest.nativeExecution.tokenizerAssetRole.isEmpty()
        || !manifest.nativeExecution.tokenizerMergesAssetRole.isEmpty()
        || !manifest.nativeExecution.frontend.isEmpty()
        || !manifest.nativeExecution.searchPolicy.isEmpty() || !manifest.nativeExecution.timestampMode.isEmpty()) {
        root.insert(QStringLiteral("native_execution"), nativeExecutionToJson(manifest.nativeExecution));
    }

    QJsonArray compatibleEngines;
    for (const ModelCompatibilityMarker &marker : manifest.compatibleEngines) {
        compatibleEngines.append(QJsonObject{
            {QStringLiteral("engine"), marker.engine},
            {QStringLiteral("model_format"), marker.modelFormat},
        });
    }
    root.insert(QStringLiteral("compatible_engines"), compatibleEngines);

    QJsonArray assets;
    for (const ModelAssetMetadata &asset : manifest.assets) {
        assets.append(QJsonObject{
            {QStringLiteral("role"), asset.role},
            {QStringLiteral("path"), asset.relativePath},
            {QStringLiteral("sha256"), asset.sha256},
            {QStringLiteral("size_bytes"), static_cast<qint64>(asset.sizeBytes)},
        });
    }
    root.insert(QStringLiteral("assets"), assets);
    setIfNotEmpty(&root, QStringLiteral("source_artifact"), manifest.sourceArtifact);
    return root;
}

std::optional<ModelPackageManifest> modelPackageManifestFromJson(const QJsonObject &root, QString *errorMessage)
{
    ModelPackageManifest manifest;
    manifest.format = readString(root, QStringLiteral("format"));
    manifest.schemaVersion = readInt(root, QStringLiteral("schema_version"));
    manifest.metadata = metadataFromJson(root.value(QStringLiteral("metadata")).toObject());
    manifest.nativeExecution = nativeExecutionFromJson(root.value(QStringLiteral("native_execution")).toObject());
    manifest.sourceArtifact = readString(root, QStringLiteral("source_artifact"));

    const QJsonArray compatibleArray = root.value(QStringLiteral("compatible_engines")).toArray();
    manifest.compatibleEngines.reserve(static_cast<std::size_t>(compatibleArray.size()));
    for (const auto &value : compatibleArray) {
        const QJsonObject object = value.toObject();
        manifest.compatibleEngines.push_back(ModelCompatibilityMarker{
            .engine = readString(object, QStringLiteral("engine")),
            .modelFormat = readString(object, QStringLiteral("model_format")),
        });
    }

    const QJsonArray assetArray = root.value(QStringLiteral("assets")).toArray();
    manifest.assets.reserve(static_cast<std::size_t>(assetArray.size()));
    for (const auto &value : assetArray) {
        const QJsonObject object = value.toObject();
        manifest.assets.push_back(ModelAssetMetadata{
            .role = readString(object, QStringLiteral("role")),
            .relativePath = readString(object, QStringLiteral("path")),
            .sha256 = readString(object, QStringLiteral("sha256")).toLower(),
            .sizeBytes = object.value(QStringLiteral("size_bytes")).toInteger(-1),
        });
    }

    if (manifest.format.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Manifest is missing format");
        }
        return std::nullopt;
    }

    return manifest;
}
