#pragma once

#include "config.h"
#include "transcription/modelpackage.h"

#include <cstdint>
#include <optional>

/**
 * @file
 * @brief Runtime-selection policy for product-owned and legacy transcription engines.
 */

/**
 * @brief Stable runtime families selectable by the app-owned factory.
 */
enum class RuntimeSelectionKind : std::uint8_t {
    CpuReference,
    LegacyWhisper,
};

/**
 * @brief Result of selecting a runtime for a configured model path.
 */
struct RuntimeSelection {
    /// Chosen runtime implementation.
    RuntimeSelectionKind kind = RuntimeSelectionKind::CpuReference;
    /// Human-readable reason suitable for diagnostics.
    QString reason;
    /// Best-effort inspected package used to make the decision.
    std::optional<ValidatedModelPackage> inspectedPackage;
};

/**
 * @brief Chooses the runtime implementation for a transcription config.
 * @param config Runtime configuration containing the model path.
 * @return Product-owned selection result with a diagnostic reason.
 */
[[nodiscard]] RuntimeSelection selectRuntimeForConfig(const TranscriberConfig &config);
