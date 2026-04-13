#pragma once

#include "asr/model/modelpackage.h"
#include "asr/runtime/transcriptiontypes.h"

#include <optional>

/**
 * @file
 * @brief Product-owned discovery and inspection helpers for model artifacts.
 */

/**
 * @brief Product-owned entrypoint for inspecting and resolving model artifacts.
 */
class ModelCatalog final
{
public:
    /**
     * @brief Inspects and resolves a model artifact path into a validated package value.
     * @param path Package root, manifest path, or raw compatibility artifact.
     * @param requiredEngine Optional engine compatibility filter.
     * @param requiredModelFormat Optional model-format compatibility filter.
     * @param error Optional destination for structured failures.
     * @return Validated package description on success.
     */
    [[nodiscard]] static std::optional<ValidatedModelPackage>
    inspectPath(const QString &path,
                QStringView requiredEngine = {},
                QStringView requiredModelFormat = {},
                RuntimeError *error = nullptr);
};
