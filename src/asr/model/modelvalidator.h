#pragma once

#include "asr/model/modelpackage.h"
#include "asr/runtime/transcriptiontypes.h"

#include <optional>

/**
 * @file
 * @brief Validation helpers for product-owned model packages.
 */

/**
 * @brief Hard bounds applied while validating model packages.
 */
struct ModelValidationLimits {
    /// Maximum accepted `model.json` size in bytes.
    qint64 maxManifestBytes = 64LL * 1024LL;
    /// Maximum number of asset entries allowed in one manifest.
    qint64 maxAssetCount = 16;
    /// Maximum total package size in bytes.
    qint64 maxPackageBytes = 8LL * 1024 * 1024 * 1024;
    /// Maximum weights asset size in bytes.
    qint64 maxWeightsBytes = 8LL * 1024 * 1024 * 1024;
};

/**
 * @brief Validates native Mutterkey model packages before runtime loading.
 */
class ModelValidator final
{
public:
    /**
     * @brief Returns the default validation limits for native packages.
     * @return Default hard validation bounds.
     */
    [[nodiscard]] static ModelValidationLimits defaultLimits();

    /**
     * @brief Validates a native model package on disk.
     * @param path Package root directory or manifest path.
     * @param requiredEngine Optional engine compatibility filter.
     * @param requiredModelFormat Optional model-format compatibility filter.
     * @param error Optional destination for structured validation failures.
     * @param limits Validation bounds to enforce.
     * @return Fully validated package on success.
     */
    [[nodiscard]] static std::optional<ValidatedModelPackage>
    validatePackagePath(const QString &path,
                        QStringView requiredEngine = {},
                        QStringView requiredModelFormat = {},
                        RuntimeError *error = nullptr,
                        const ModelValidationLimits &limits = defaultLimits());
};
