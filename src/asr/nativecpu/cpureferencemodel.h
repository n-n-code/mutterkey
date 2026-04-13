#pragma once

#include "asr/model/modelpackage.h"
#include "asr/nativecpu/cpudecodergraph.h"
#include "asr/nativecpu/cpuwhispertokenizer.h"
#include "asr/runtime/transcriptionengine.h"
#include "asr/runtime/transcriptiontypes.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

struct CpuWhisperModelWeights;

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
    /// Version-specific payload field 1.
    std::uint32_t payloadField1 = 0;
    /// Version-specific payload field 2.
    std::uint32_t payloadField2 = 0;
    /// Version-specific payload field 3.
    std::uint32_t payloadField3 = 0;
};

/**
 * @brief Supported native CPU weights payload shapes.
 */
enum class CpuReferenceModelKind : std::uint8_t {
    FixtureV1,
    TemplateDecoderScaffoldV2,
    BaselineFamilyDecoderV2,
    RealDecoderV3,
};

/**
 * @brief Deterministic or template-backed native CPU model payload loaded from disk.
 */
struct CpuReferenceModelData {
    /// Loaded native CPU payload flavor.
    CpuReferenceModelKind kind = CpuReferenceModelKind::FixtureV1;
    /// Final transcript emitted by the older deterministic reference fixture format.
    QString transcript;
    /// Fixed feature profile size used by template-matching models.
    int featureBinCount = 0;
    /// Maximum accepted template distance for a successful match.
    float maxDistance = 0.0F;
    /// Phrase templates available in a template-matching model.
    std::vector<CpuDecodedPhraseTemplate> phraseTemplates;
    /// Optional packaged token vocabulary used for stable token ids.
    std::vector<QString> tokenVocabulary;
    /// Optional packaged Whisper-family tokenizer used by real decoder packages.
    std::optional<CpuWhisperTokenizerModel> whisperTokenizer;
    /// Loaded tensor weights for the real decoder path (v3).
    std::shared_ptr<CpuWhisperModelWeights> tensorWeights;
};

/**
 * @brief Immutable decoder-facing execution metadata derived from the package manifest.
 */
struct CpuReferenceExecutionMetadata {
    /// Decoder implementation marker.
    QString decoder;
    /// Tokenizer contract marker.
    QString tokenizer;
    /// Frozen baseline family marker such as `whisper-base-en`.
    QString baselineFamily;
    /// Tokenizer asset role declared by the package.
    QString tokenizerAssetRole;
    /// Optional tokenizer merge-asset role declared by the package.
    QString tokenizerMergesAssetRole;
    /// Frontend contract marker.
    QString frontend;
    /// Search-policy contract marker.
    QString searchPolicy;
    /// Timestamp-mode contract marker.
    QString timestampMode;
    /// Feature-bin count expected by the decoder.
    int featureBinCount = 0;
    /// Template count declared by the package manifest.
    int templateCount = 0;
    /// Max match distance declared by the package manifest.
    double maxDistance = 0.0;
    /// Decoder begin-of-transcript token id.
    int bosTokenId = -1;
    /// Decoder end-of-transcript token id.
    int eosTokenId = -1;
    /// Decoder no-speech token id.
    int noSpeechTokenId = -1;
    /// First timestamp token id.
    int timestampTokenStartId = -1;
    /// Last timestamp token id.
    int timestampTokenEndId = -1;
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
     * @brief Returns the parsed native CPU model payload.
     * @return Immutable payload used by the session-side decoder.
     */
    [[nodiscard]] const CpuReferenceModelData &model() const;

    /**
     * @brief Returns decoder-facing execution metadata from the validated package.
     * @return Immutable execution contract used to cross-check payload shape.
     */
    [[nodiscard]] const CpuReferenceExecutionMetadata &execution() const;

private:
    ValidatedModelPackage m_package;
    CpuReferenceModelData m_model;
    CpuReferenceExecutionMetadata m_execution;
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
