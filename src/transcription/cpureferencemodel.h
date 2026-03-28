#pragma once

#include "transcription/modelpackage.h"
#include "transcription/transcriptionengine.h"
#include "transcription/transcriptiontypes.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

/**
 * @file
 * @brief Native CPU reference model format definitions and loading helpers.
 */

/**
 * @brief Fixed header stored at the start of a native CPU reference weights asset.
 */
struct CpuReferenceModelHeader {
    /// File-format magic. Kept ASCII for stable inspection and tests.
    std::array<char, 8> magic{};
    /// Native CPU reference weights format version.
    std::uint32_t version = 0;
    /// UTF-8 transcript payload size used by the deterministic reference fixture format.
    std::uint32_t transcriptBytes = 0;
};

/**
 * @brief Deterministic native CPU reference model payload loaded from disk.
 */
struct CpuReferenceModelData {
    /// Final transcript emitted by the deterministic reference runtime fixture.
    QString transcript;
};

/**
 * @brief Immutable loaded native CPU model handle owned by the runtime.
 */
class CpuReferenceModelHandle final : public TranscriptionModelHandle
{
public:
    /**
     * @brief Creates an immutable native CPU reference model handle.
     * @param package Validated package metadata and resolved asset paths.
     * @param model Parsed deterministic reference model payload.
     * @param loadTimeMs Measured load time in milliseconds for diagnostics.
     */
    CpuReferenceModelHandle(ValidatedModelPackage package, CpuReferenceModelData model, qint64 loadTimeMs);

    [[nodiscard]] QString backendName() const override;
    [[nodiscard]] ModelMetadata metadata() const override;
    [[nodiscard]] QString modelDescription() const override;

    /**
     * @brief Returns the deterministic transcript fixture embedded in this model.
     * @return Transcript text emitted by the reference runtime.
     */
    [[nodiscard]] const QString &transcript() const;

private:
    ValidatedModelPackage m_package;
    CpuReferenceModelData m_model;
    qint64 m_loadTimeMs = 0;
};

/**
 * @brief Loads a native CPU reference model handle from a validated package.
 * @param package Validated native package resolved by the model catalog.
 * @param error Optional output for format or IO failures.
 * @return Shared immutable model handle on success.
 */
[[nodiscard]] std::shared_ptr<const CpuReferenceModelHandle>
loadCpuReferenceModelHandle(const ValidatedModelPackage &package, RuntimeError *error = nullptr);

/**
 * @brief Downcasts a generic model handle to the native CPU reference handle type.
 * @param model Shared generic runtime model handle.
 * @return Native CPU reference model handle on success.
 */
[[nodiscard]] std::shared_ptr<const CpuReferenceModelHandle>
resolveCpuReferenceModelHandle(std::shared_ptr<const TranscriptionModelHandle> model);
