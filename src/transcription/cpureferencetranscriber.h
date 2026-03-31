#pragma once

#include "config.h"
#include "transcription/cpureferencemodel.h"
#include "transcription/cpusessionstate.h"
#include "transcription/modelpackage.h"
#include "transcription/transcriptionengine.h"
#include "transcription/transcriptiontypes.h"

#include <memory>

/**
 * @file
 * @brief Product-owned deterministic CPU reference runtime.
 */

/**
 * @brief In-process native CPU transcription backend used as the product-owned reference runtime.
 *
 * The Phase 5 reference path is intentionally narrow and deterministic. It owns
 * its model parsing, session state, and transcript emission without exposing any
 * third-party runtime handles to the rest of the app.
 */
class CpuReferenceTranscriber final : public TranscriptionSession
{
public:
    /**
     * @brief Loads an immutable native CPU model handle from a validated package.
     * @param config Runtime configuration copied into the handle.
     * @param error Optional output for model validation or load failures.
     * @return Shared immutable model handle on success.
     */
    [[nodiscard]] static std::shared_ptr<const TranscriptionModelHandle>
    loadModelHandle(const TranscriberConfig &config, RuntimeError *error = nullptr);

    /**
     * @brief Creates a mutable session from a generic app-owned model handle.
     * @param config Runtime configuration copied into the session.
     * @param model Shared immutable model handle.
     * @return Session on success, otherwise `nullptr` when the model type is incompatible.
     */
    [[nodiscard]] static std::unique_ptr<TranscriptionSession>
    createSession(TranscriberConfig config, std::shared_ptr<const TranscriptionModelHandle> model);

    /**
     * @brief Creates a mutable native CPU session from a loaded model handle.
     * @param config Runtime configuration copied into the session.
     * @param model Shared immutable model handle created by the native CPU engine.
     */
    CpuReferenceTranscriber(TranscriberConfig config, std::shared_ptr<const TranscriptionModelHandle> model);

    ~CpuReferenceTranscriber() override;

    CpuReferenceTranscriber(const CpuReferenceTranscriber &) = delete;
    CpuReferenceTranscriber &operator=(const CpuReferenceTranscriber &) = delete;
    CpuReferenceTranscriber(CpuReferenceTranscriber &&) = delete;
    CpuReferenceTranscriber &operator=(CpuReferenceTranscriber &&) = delete;

    /**
     * @brief Returns the stable backend identifier used in diagnostics.
     * @return Product-owned backend name for the CPU reference runtime.
     */
    [[nodiscard]] static QString backendNameStatic();

    /**
     * @brief Returns static capability metadata for the native CPU runtime.
     * @return Engine-owned capability snapshot.
     */
    [[nodiscard]] static BackendCapabilities capabilitiesStatic();

    /**
     * @brief Returns static runtime diagnostics for the native CPU runtime.
     * @return Engine-owned diagnostic description.
     */
    [[nodiscard]] static RuntimeDiagnostics diagnosticsStatic();

    [[nodiscard]] QString backendName() const override;
    bool warmup(RuntimeError *error = nullptr) override;
    [[nodiscard]] TranscriptUpdate pushAudioChunk(const AudioChunk &chunk) override;
    [[nodiscard]] TranscriptUpdate finish() override;
    [[nodiscard]] TranscriptUpdate cancel() override;

private:
    TranscriberConfig m_config;
    std::shared_ptr<const CpuReferenceModelHandle> m_model;
    CpuSessionState m_state;
};

/**
 * @brief Creates the product-owned CPU reference engine implementation.
 * @param config Runtime configuration copied into the engine.
 * @return Engine backed by the native CPU reference runtime.
 */
[[nodiscard]] std::shared_ptr<const TranscriptionEngine> createCpuReferenceTranscriptionEngine(const TranscriberConfig &config);
