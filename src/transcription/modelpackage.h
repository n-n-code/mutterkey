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
 * @brief Native execution metadata for app-owned decoder packages.
 *
 * This stays separate from user-facing package metadata so runtime invariants
 * can evolve without overloading the generic inspection surface.
 */
struct NativeExecutionMetadata {
    /// Native execution contract version within the packaged model family.
    int executionVersion = 0;
    /// Frozen baseline family marker such as `whisper-base-en`.
    QString baselineFamily;
    /// Decoder implementation family such as `template-matcher`.
    QString decoder;
    /// Tokenizer contract marker such as `phrase-template`.
    QString tokenizer;
    /// Asset role that carries the packaged tokenizer or vocabulary payload.
    QString tokenizerAssetRole;
    /// Optional asset role that carries tokenizer merge rules for BPE tokenizers.
    QString tokenizerMergesAssetRole;
    /// Frontend feature extractor marker such as `energy-profile-v1`.
    QString frontend;
    /// Search-policy marker such as `greedy-template-v1`.
    QString searchPolicy;
    /// Timestamp-mode marker such as `utterance-duration-v1`.
    QString timestampMode;
    /// Number of expected feature bins per utterance.
    int featureBinCount = 0;
    /// Phrase/template count when known.
    int templateCount = 0;
    /// Quantized max-distance threshold scaled in manifest-friendly decimal form.
    double maxDistance = 0.0;
    /// Decoder begin-of-transcript token id when known.
    int bosTokenId = -1;
    /// Decoder end-of-transcript token id when known.
    int eosTokenId = -1;
    /// Decoder no-speech token id when known.
    int noSpeechTokenId = -1;
    /// First timestamp token id when known.
    int timestampTokenStartId = -1;
    /// Last timestamp token id when known.
    int timestampTokenEndId = -1;
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
    /// Native execution metadata for app-owned decoder packages.
    NativeExecutionMetadata nativeExecution;
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
     * @brief Returns native execution metadata for app-owned decoder packages.
     * @return Decoder-facing manifest metadata separate from generic package metadata.
     */
    [[nodiscard]] const NativeExecutionMetadata &nativeExecution() const { return manifest.nativeExecution; }

    /**
     * @brief Finds one packaged asset entry by its logical role.
     * @param role Logical asset role such as `weights` or `tokenizer`.
     * @return Matching asset metadata when present.
     */
    [[nodiscard]] std::optional<ModelAssetMetadata> assetByRole(QStringView role) const;

    /**
     * @brief Resolves one packaged asset path by its logical role.
     * @param role Logical asset role such as `weights` or `tokenizer`.
     * @return Absolute asset path when the manifest contains that role.
     */
    [[nodiscard]] std::optional<QString> resolvedAssetPath(QStringView role) const;

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
 * @brief Stable engine identifier for the native CPU reference runtime.
 * @return Product-owned engine marker recorded in package manifests.
 */
[[nodiscard]] QString cpuReferenceEngineName();

/**
 * @brief Stable model-format identifier for the native CPU reference runtime.
 * @return Product-owned model-format marker recorded in package manifests.
 */
[[nodiscard]] QString cpuReferenceModelFormat();

/**
 * @brief Stable legacy fixture model-format identifier for older native CPU packages.
 * @return Product-owned model-format marker recorded in older fixture manifests.
 */
[[nodiscard]] QString cpuReferenceFixtureModelFormat();

/**
 * @brief Stable engine identifier for the legacy whisper.cpp adapter.
 * @return Legacy engine marker recorded in package manifests.
 */
[[nodiscard]] QString legacyWhisperEngineName();

/**
 * @brief Stable model-format identifier for the legacy whisper.cpp adapter.
 * @return Legacy model-format marker recorded in package manifests.
 */
[[nodiscard]] QString legacyWhisperModelFormat();

/**
 * @brief Reports whether a manifest advertises compatibility with a runtime marker pair.
 * @param manifest Parsed model package manifest.
 * @param engine Stable engine identifier to match.
 * @param modelFormat Stable model-format marker to match.
 * @return `true` when the manifest contains a matching compatibility marker.
 */
[[nodiscard]] bool modelPackageSupportsCompatibility(const ModelPackageManifest &manifest,
                                                     QStringView engine,
                                                     QStringView modelFormat);

/**
 * @brief Finds one packaged asset entry by its logical role.
 * @param manifest Parsed model package manifest.
 * @param role Logical asset role such as `weights` or `tokenizer`.
 * @return Matching asset metadata when present.
 */
[[nodiscard]] std::optional<ModelAssetMetadata> modelPackageAssetByRole(const ModelPackageManifest &manifest, QStringView role);

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
