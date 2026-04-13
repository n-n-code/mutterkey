#include "asr/nativecpu/cpureferencetranscriber.h"

#include "asr/nativecpu/cpudecoderruntime.h"
#include "asr/nativecpu/cpufeatureextractor.h"
#include "asr/model/modelcatalog.h"
#include "asr/runtime/runtimeselector.h"

#include <QLoggingCategory>
#include <QThread>

#include <algorithm>
#include <utility>

namespace {

Q_LOGGING_CATEGORY(cpuReferenceLog, "mutterkey.transcriber.cpu_reference")

constexpr int kExpectedSampleRate = 16000;
constexpr float kMinimumSpeechPeak = 0.02F;

RuntimeError makeRuntimeError(RuntimeErrorCode code, QString message, QString detail = {})
{
    return RuntimeError{.code = code, .message = std::move(message), .detail = std::move(detail)};
}

RuntimeError makeUnsupportedLanguageError(const QString &language)
{
    return makeRuntimeError(RuntimeErrorCode::UnsupportedLanguage,
                            QStringLiteral("Native CPU reference runtime does not support language: %1").arg(language),
                            QStringLiteral("Requested language code: %1").arg(language));
}

QString normalizedLanguage(const QString &value)
{
    return value.trimmed().toLower();
}

bool supportsLanguage(const QString &languageProfile, const QString &requestedLanguage)
{
    if (requestedLanguage.isEmpty() || requestedLanguage == QStringLiteral("auto")) {
        return !languageProfile.isEmpty();
    }

    if (languageProfile == QStringLiteral("multilingual")) {
        return true;
    }

    return requestedLanguage == languageProfile;
}

} // namespace

std::shared_ptr<const TranscriptionModelHandle>
CpuReferenceTranscriber::loadModelHandle(const TranscriberConfig &config, RuntimeError *error)
{
    const auto loadForFormat = [&config](QStringView modelFormat, RuntimeError *runtimeError) {
        return ModelCatalog::inspectPath(config.modelPath, cpuReferenceEngineName(), modelFormat, runtimeError);
    };

    RuntimeError formatError;
    std::optional<ValidatedModelPackage> package = loadForFormat(cpuReferenceModelFormat(), &formatError);
    if (!package.has_value()) {
        package = loadForFormat(cpuReferenceFixtureModelFormat(), &formatError);
    }
    if (!package.has_value()) {
        if (error != nullptr) {
            *error = formatError;
        }
        return nullptr;
    }

    qCInfo(cpuReferenceLog) << "Loaded native CPU reference model from" << package->weightsPath;
    return loadCpuReferenceModelHandle(*package, error);
}

std::unique_ptr<TranscriptionSession>
CpuReferenceTranscriber::createSession(TranscriberConfig config, std::shared_ptr<const TranscriptionModelHandle> model)
{
    std::shared_ptr<const CpuReferenceModelHandle> cpuModel = resolveCpuReferenceModelHandle(std::move(model));
    if (cpuModel == nullptr) {
        return nullptr;
    }

    return std::make_unique<CpuReferenceTranscriber>(std::move(config), std::move(cpuModel));
}

CpuReferenceTranscriber::CpuReferenceTranscriber(TranscriberConfig config, std::shared_ptr<const TranscriptionModelHandle> model)
    : m_config(std::move(config))
    , m_model(resolveCpuReferenceModelHandle(std::move(model)))
{
}

CpuReferenceTranscriber::~CpuReferenceTranscriber() = default;

QString CpuReferenceTranscriber::backendNameStatic()
{
    return cpuReferenceEngineName();
}

BackendCapabilities CpuReferenceTranscriber::capabilitiesStatic()
{
    return BackendCapabilities{
        .backendName = backendNameStatic(),
        .supportedLanguages = {QStringLiteral("en")},
        .supportsAutoLanguage = false,
        .supportsTranslation = false,
        .supportsWarmup = true,
    };
}

RuntimeDiagnostics CpuReferenceTranscriber::diagnosticsStatic()
{
    return RuntimeDiagnostics{
        .backendName = backendNameStatic(),
        .selectionReason = QStringLiteral("Native CPU decoder runtime selected explicitly"),
        .runtimeDescription = QStringLiteral("native cpu decoder runtime; threads=%1")
                                  .arg(std::max(1, QThread::idealThreadCount())),
        .loadedModelDescription = {},
    };
}

QString CpuReferenceTranscriber::backendName() const
{
    return backendNameStatic();
}

bool CpuReferenceTranscriber::warmup(RuntimeError *error)
{
    if (m_model == nullptr) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::InternalRuntimeError,
                                      QStringLiteral("Native CPU reference model handle is not available"));
        }
        return false;
    }

    if (m_model->model().kind == CpuReferenceModelKind::RealDecoderV3
        && m_model->model().tensorWeights != nullptr) {
        m_state.allocateKVCache(m_model->model().tensorWeights->config, 224);
    }

    m_state.markWarmedUp();
    return true;
}

TranscriptUpdate CpuReferenceTranscriber::pushAudioChunk(const AudioChunk &chunk)
{
    if (!chunk.isValid()) {
        return TranscriptUpdate{
            .events = {},
            .error = makeRuntimeError(RuntimeErrorCode::AudioNormalizationFailed,
                                      QStringLiteral("Streaming audio chunk is empty")),
        };
    }

    if (chunk.sampleRate != kExpectedSampleRate || chunk.channels != 1) {
        return TranscriptUpdate{
            .events = {},
            .error = makeRuntimeError(RuntimeErrorCode::AudioNormalizationFailed,
                                      QStringLiteral("Native CPU reference runtime requires 16 kHz mono chunks")),
        };
    }

    m_state.appendChunk(chunk);
    return {};
}

TranscriptUpdate CpuReferenceTranscriber::finish()
{
    if (m_state.cancelRequested()) {
        m_state.resetForNextUtterance();
        return TranscriptUpdate{
            .events = {},
            .error = makeRuntimeError(RuntimeErrorCode::Cancelled,
                                      QStringLiteral("Native CPU reference transcription was cancelled")),
        };
    }

    if (m_model == nullptr) {
        return TranscriptUpdate{
            .events = {},
            .error = makeRuntimeError(RuntimeErrorCode::InternalRuntimeError,
                                      QStringLiteral("Native CPU reference model handle is not available")),
        };
    }

    if (m_config.translate) {
        return TranscriptUpdate{
            .events = {},
            .error = makeRuntimeError(RuntimeErrorCode::InvalidConfig,
                                      QStringLiteral("Native CPU reference runtime does not support translation mode")),
        };
    }

    const QString requestedLanguage = normalizedLanguage(m_config.language);
    if (!supportsLanguage(m_model->metadata().languageProfile, requestedLanguage)) {
        return TranscriptUpdate{
            .events = {},
            .error = makeUnsupportedLanguageError(requestedLanguage),
        };
    }

    if (!m_state.isWarmedUp()) {
        RuntimeError warmupError;
        if (!warmup(&warmupError)) {
            return TranscriptUpdate{.events = {}, .error = std::move(warmupError)};
        }
    }

    const std::vector<float> &bufferedSamples = m_state.bufferedSamples();
    if (bufferedSamples.empty() || peakAbsoluteSample(bufferedSamples) < kMinimumSpeechPeak) {
        m_state.resetForNextUtterance();
        return {};
    }

    const std::optional<CpuDecodeResult> decodeResult = runCpuDecodePass(CpuDecodeRequest{
        .samples = bufferedSamples,
        .model = &m_model->model(),
        .execution = &m_model->execution(),
        .kvCache = m_model->model().kind == CpuReferenceModelKind::RealDecoderV3
            ? &m_state.kvCache() : nullptr,
        .sampleRate = kExpectedSampleRate,
    });
    m_state.resetForNextUtterance();
    if (!decodeResult.has_value()) {
        return {};
    }

    return TranscriptUpdate{
        .events = {
            decodeResult->event,
        },
        .error = {},
    };
}

TranscriptUpdate CpuReferenceTranscriber::cancel()
{
    m_state.requestCancel();
    return TranscriptUpdate{
        .events = {},
        .error = makeRuntimeError(RuntimeErrorCode::Cancelled,
                                  QStringLiteral("Native CPU reference transcription was cancelled")),
    };
}

namespace {

class CpuReferenceTranscriptionEngine final : public TranscriptionEngine
{
public:
    explicit CpuReferenceTranscriptionEngine(TranscriberConfig config)
        : m_config(std::move(config))
    {
    }

    [[nodiscard]] BackendCapabilities capabilities() const override
    {
        return CpuReferenceTranscriber::capabilitiesStatic();
    }

    [[nodiscard]] RuntimeDiagnostics diagnostics() const override
    {
        RuntimeDiagnostics diagnostics = CpuReferenceTranscriber::diagnosticsStatic();
        diagnostics.selectionReason = selectRuntimeForConfig(m_config).reason;
        return diagnostics;
    }

    [[nodiscard]] std::shared_ptr<const TranscriptionModelHandle> loadModel(RuntimeError *error) const override
    {
        return CpuReferenceTranscriber::loadModelHandle(m_config, error);
    }

    [[nodiscard]] std::unique_ptr<TranscriptionSession>
    createSession(std::shared_ptr<const TranscriptionModelHandle> model) const override
    {
        return CpuReferenceTranscriber::createSession(m_config, std::move(model));
    }

private:
    TranscriberConfig m_config;
};

} // namespace

std::shared_ptr<const TranscriptionEngine> createCpuReferenceTranscriptionEngine(const TranscriberConfig &config)
{
    return std::make_shared<CpuReferenceTranscriptionEngine>(config);
}
