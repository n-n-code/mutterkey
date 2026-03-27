#pragma once

#include "transcription/transcriptiontypes.h"

#include <QJsonObject>

#include <optional>
#include <vector>

/**
 * @file
 * @brief Product-owned model package manifest and validated package value types.
 */

/**
 * @brief One engine/model-format compatibility marker recorded in a package manifest.
 */
struct ModelCompatibilityMarker {
    /// Stable engine identifier accepted by this package.
    QString engine;
    /// Engine-specific model format marker such as `ggml`.
    QString modelFormat;
};

/**
 * @brief One packaged asset entry recorded in a package manifest.
 */
struct ModelAssetMetadata {
    /// Logical asset role such as `weights`.
    QString role;
    /// Relative path from the package root to the asset file.
    QString relativePath;
    /// Lowercase SHA-256 digest for the asset contents.
    QString sha256;
    /// Declared size of the asset in bytes.
    qint64 sizeBytes = 0;
};

/**
 * @brief Product-owned manifest data parsed from `model.json`.
 */
struct ModelPackageManifest {
    /// Stable package format identifier.
    QString format;
    /// Schema version for this manifest.
    int schemaVersion = 0;
    /// Product-owned immutable metadata about the packaged model.
    ModelMetadata metadata;
    /// Compatible engines and model formats for this package.
    std::vector<ModelCompatibilityMarker> compatibleEngines;
    /// Packaged assets referenced by this manifest.
    std::vector<ModelAssetMetadata> assets;
    /// Optional source artifact description used for diagnostics.
    QString sourceArtifact;
};

/**
 * @brief Fully validated model package resolved from disk.
 */
struct ValidatedModelPackage {
    /// Resolved package root directory.
    QString packageRootPath;
    /// Resolved `model.json` path when the artifact is a native package.
    QString manifestPath;
    /// Original user-provided source path resolved to an absolute path.
    QString sourcePath;
    /// Resolved weights asset path used by the backend adapter.
    QString weightsPath;
    /// Parsed product-owned manifest data.
    ModelPackageManifest manifest;

    /**
     * @brief Returns the immutable model metadata carried by this package.
     * @return Product-owned metadata for the resolved artifact.
     */
    [[nodiscard]] const ModelMetadata &metadata() const { return manifest.metadata; }

    /**
     * @brief Reports whether this package came from the legacy raw-file compatibility path.
     * @return `true` for raw Whisper compatibility artifacts.
     */
    [[nodiscard]] bool isLegacyCompatibility() const { return manifest.metadata.legacyCompatibility; }

    /**
     * @brief Returns a human-readable description for diagnostics and logs.
     * @return Display label derived from package metadata and source path.
     */
    [[nodiscard]] QString description() const;
};

/**
 * @brief Returns the default root directory for native model packages.
 * @return Default package directory under the app data root.
 */
[[nodiscard]] QString defaultModelPackageDirectory();

/**
 * @brief Normalizes a human-provided package id into a stable filesystem-safe form.
 * @param value Raw package id or display string.
 * @return Lowercase sanitized package id.
 */
[[nodiscard]] QString sanitizePackageId(const QString &value);

/**
 * @brief Serializes a product-owned package manifest to JSON.
 * @param manifest Manifest value to serialize.
 * @return JSON object suitable for writing to `model.json`.
 */
[[nodiscard]] QJsonObject modelPackageManifestToJson(const ModelPackageManifest &manifest);

/**
 * @brief Parses a product-owned package manifest from JSON.
 * @param root JSON object read from `model.json`.
 * @param errorMessage Optional destination for parse failures.
 * @return Parsed manifest on success.
 */
[[nodiscard]] std::optional<ModelPackageManifest> modelPackageManifestFromJson(const QJsonObject &root, QString *errorMessage = nullptr);
