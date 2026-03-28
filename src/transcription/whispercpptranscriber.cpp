#include "transcription/whispercpptranscriber.h"

#include "transcription/modelcatalog.h"
#include "transcription/runtimeselector.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <utility>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStringList>
#include <QThread>

extern "C" {
#include <ggml-backend.h>
#include <whisper.h>
}

namespace {

Q_LOGGING_CATEGORY(whisperCppLog, "mutterkey.transcriber.whispercpp")
} // namespace

class WhisperCppModelHandle final : public TranscriptionModelHandle
{
public:
    WhisperCppModelHandle(ValidatedModelPackage package,
                          std::unique_ptr<whisper_context, void (*)(whisper_context *)> context)
        : m_package(std::move(package))
        , m_context(std::move(context))
    {
    }

    [[nodiscard]] whisper_context *context() const
    {
        return m_context.get();
    }

    [[nodiscard]] const QString &modelPath() const
    {
        return m_package.weightsPath;
    }

    [[nodiscard]] QString backendName() const override
    {
        return WhisperCppTranscriber::backendNameStatic();
    }

    [[nodiscard]] ModelMetadata metadata() const override
    {
        return m_package.metadata();
    }

    [[nodiscard]] QString modelDescription() const override
    {
        return m_package.description();
    }

private:
    ValidatedModelPackage m_package;
    std::unique_ptr<whisper_context, void (*)(whisper_context *)> m_context;
};

namespace {

std::shared_ptr<const WhisperCppModelHandle>
resolveWhisperModelHandle(std::shared_ptr<const TranscriptionModelHandle> model)
{
    return std::dynamic_pointer_cast<const WhisperCppModelHandle>(std::move(model));
}

QString backendDeviceTypeName(enum ggml_backend_dev_type type)
{
    switch (type) {
    case GGML_BACKEND_DEVICE_TYPE_CPU:
        return QStringLiteral("CPU");
    case GGML_BACKEND_DEVICE_TYPE_GPU:
        return QStringLiteral("GPU");
    case GGML_BACKEND_DEVICE_TYPE_IGPU:
        return QStringLiteral("IGPU");
    case GGML_BACKEND_DEVICE_TYPE_ACCEL:
        return QStringLiteral("ACCEL");
    }

    return QStringLiteral("UNKNOWN");
}

QString describeRegisteredBackends()
{
    QStringList backendNames;
    backendNames.reserve(static_cast<qsizetype>(ggml_backend_reg_count()));
    for (size_t index = 0; index < ggml_backend_reg_count(); ++index) {
        if (ggml_backend_reg_t reg = ggml_backend_reg_get(index)) {
            backendNames.append(QString::fromUtf8(ggml_backend_reg_name(reg)));
        }
    }

    QStringList deviceDescriptions;
    deviceDescriptions.reserve(static_cast<qsizetype>(ggml_backend_dev_count()));
    for (size_t index = 0; index < ggml_backend_dev_count(); ++index) {
        if (ggml_backend_dev_t device = ggml_backend_dev_get(index)) {
            const QString deviceName = QString::fromUtf8(ggml_backend_dev_name(device));
            const QString deviceDescription = QString::fromUtf8(ggml_backend_dev_description(device));
            deviceDescriptions.append(QStringLiteral("%1[%2]: %3")
                                          .arg(deviceName,
                                               backendDeviceTypeName(ggml_backend_dev_type(device)),
                                               deviceDescription));
        }
    }

    return QStringLiteral("registered backends=%1; devices=%2")
        .arg(backendNames.join(QStringLiteral(", ")), deviceDescriptions.join(QStringLiteral(" | ")));
}

RuntimeError makeRuntimeError(RuntimeErrorCode code, QString message, QString detail = {})
{
    return RuntimeError{.code = code, .message = std::move(message), .detail = std::move(detail)};
}

RuntimeError makeUnsupportedLanguageError(const QString &language)
{
    return makeRuntimeError(RuntimeErrorCode::UnsupportedLanguage,
                            QStringLiteral("Embedded Whisper does not support language: %1").arg(language),
                            QStringLiteral("Requested language code: %1").arg(language));
}

RuntimeError makeCancelledError()
{
    return makeRuntimeError(RuntimeErrorCode::Cancelled, QStringLiteral("Embedded Whisper transcription was cancelled"));
}

TranscriptUpdate makeCancelledUpdate()
{
    return TranscriptUpdate{.events = {}, .error = makeCancelledError()};
}

bool isSupportedLanguageCode(const QString &language)
{
    const QString normalizedLanguage = language.trimmed().toLower();
    if (normalizedLanguage.isEmpty() || normalizedLanguage == QStringLiteral("auto")) {
        return true;
    }

    for (int languageId = 0; languageId <= whisper_lang_max_id(); ++languageId) {
        const char *languageCode = whisper_lang_str(languageId);
        if (languageCode != nullptr && normalizedLanguage == QString::fromUtf8(languageCode)) {
            return true;
        }
    }

    return false;
}

void freeContext(whisper_context *context) noexcept
{
    if (context != nullptr) {
        whisper_free(context);
    }
}

bool shouldAbortDecode(void *userData)
{
    const auto *cancelRequested = static_cast<const std::atomic_bool *>(userData);
    return cancelRequested != nullptr && cancelRequested->load(std::memory_order_relaxed);
}

} // namespace

std::shared_ptr<const TranscriptionModelHandle>
WhisperCppTranscriber::loadModelHandle(const TranscriberConfig &config, RuntimeError *error)
{
    const std::optional<ValidatedModelPackage> package =
        ModelCatalog::inspectPath(config.modelPath, legacyWhisperEngineName(), legacyWhisperModelFormat(), error);
    if (!package.has_value()) {
        return nullptr;
    }
    const QString modelPath = package->weightsPath;

    const whisper_context_params contextParams = whisper_context_default_params();
    qCInfo(whisperCppLog).noquote() << "ggml runtime:" << describeRegisteredBackends();
    std::unique_ptr<whisper_context, void (*)(whisper_context *)> context(
        whisper_init_from_file_with_params_no_state(modelPath.toUtf8().constData(), contextParams),
        &freeContext);
    if (context == nullptr) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Failed to load embedded Whisper model: %1").arg(modelPath));
        }
        return nullptr;
    }

    qCInfo(whisperCppLog) << "Loaded embedded Whisper model from" << modelPath;
    return std::make_shared<WhisperCppModelHandle>(*package, std::move(context));
}

std::unique_ptr<TranscriptionSession>
WhisperCppTranscriber::createSession(TranscriberConfig config, std::shared_ptr<const TranscriptionModelHandle> model)
{
    std::shared_ptr<const WhisperCppModelHandle> whisperModel = resolveWhisperModelHandle(std::move(model));
    if (whisperModel == nullptr) {
        return nullptr;
    }

    return std::make_unique<WhisperCppTranscriber>(std::move(config), std::move(whisperModel));
}

WhisperCppTranscriber::WhisperCppTranscriber(TranscriberConfig config, std::shared_ptr<const TranscriptionModelHandle> model)
    : m_config(std::move(config))
    , m_model(resolveWhisperModelHandle(std::move(model)))
    , m_state(nullptr, &WhisperCppTranscriber::freeState)
{
}

WhisperCppTranscriber::~WhisperCppTranscriber() = default;

void WhisperCppTranscriber::freeState(whisper_state *state) noexcept
{
    if (state != nullptr) {
        whisper_free_state(state);
    }
}

QString WhisperCppTranscriber::backendNameStatic()
{
    return QStringLiteral("whisper.cpp");
}

BackendCapabilities WhisperCppTranscriber::capabilitiesStatic()
{
    BackendCapabilities capabilities;
    capabilities.backendName = backendNameStatic();
    capabilities.supportsAutoLanguage = true;
    capabilities.supportsTranslation = true;
    capabilities.supportsWarmup = true;
    capabilities.supportedLanguages.reserve(whisper_lang_max_id() + 1);
    for (int languageId = 0; languageId <= whisper_lang_max_id(); ++languageId) {
        const char *languageCode = whisper_lang_str(languageId);
        if (languageCode != nullptr) {
            capabilities.supportedLanguages.append(QString::fromUtf8(languageCode));
        }
    }
    return capabilities;
}

RuntimeDiagnostics WhisperCppTranscriber::diagnosticsStatic()
{
    return RuntimeDiagnostics{
        .backendName = backendNameStatic(),
        .selectionReason = QStringLiteral("Legacy whisper runtime selected explicitly"),
        .runtimeDescription = describeRegisteredBackends(),
        .loadedModelDescription = {},
    };
}

QString WhisperCppTranscriber::backendName() const
{
    return backendNameStatic();
}

bool WhisperCppTranscriber::warmup(RuntimeError *error)
{
    return ensureState(error);
}

TranscriptUpdate WhisperCppTranscriber::pushAudioChunk(const AudioChunk &chunk)
{
    if (!chunk.isValid()) {
        return TranscriptUpdate{
            .events = {},
            .error = makeRuntimeError(RuntimeErrorCode::AudioNormalizationFailed,
                                      QStringLiteral("Streaming audio chunk is empty")),
        };
    }

    if (chunk.sampleRate <= 0 || chunk.channels != 1) {
        return TranscriptUpdate{
            .events = {},
            .error = makeRuntimeError(RuntimeErrorCode::AudioNormalizationFailed,
                                      QStringLiteral("Streaming audio chunk format is invalid")),
        };
    }

    if (chunk.sampleRate != 16000) {
        return TranscriptUpdate{
            .events = {},
            .error = makeRuntimeError(RuntimeErrorCode::AudioNormalizationFailed,
                                      QStringLiteral("Embedded Whisper requires 16 kHz normalized chunks")),
        };
    }

    qCInfo(whisperCppLog) << "Normalized recording to"
                          << chunk.samples.size()
                          << "mono samples at"
                          << chunk.sampleRate
                          << "Hz";
    m_bufferedSamples.insert(m_bufferedSamples.end(), chunk.samples.begin(), chunk.samples.end());
    return TranscriptUpdate{};
}

TranscriptUpdate WhisperCppTranscriber::finish()
{
    const QString requestedLanguage = m_config.language.trimmed().toLower();
    if (!isSupportedLanguageCode(requestedLanguage)) {
        return TranscriptUpdate{.events = {}, .error = makeUnsupportedLanguageError(requestedLanguage)};
    }

    RuntimeError runtimeError;
    if (!ensureState(&runtimeError)) {
        return TranscriptUpdate{.events = {}, .error = runtimeError};
    }

    if (m_bufferedSamples.empty()) {
        return TranscriptUpdate{};
    }

    // Keep the inference configuration close to the callsite so the runtime behavior is
    // obvious from the integration layer without reading whisper.cpp internals.
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress = false;
    params.print_special = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.translate = m_config.translate;
    params.single_segment = false;
    params.no_context = true;
    params.n_threads = m_config.threads > 0 ? m_config.threads : std::max(1, QThread::idealThreadCount());
    params.abort_callback = &shouldAbortDecode;
    params.abort_callback_user_data = &m_cancelRequested;

    const QByteArray language = m_config.language.trimmed().toUtf8();
    if (!language.isEmpty()) {
        params.language = language.constData();
    }

    m_cancelRequested.store(false, std::memory_order_relaxed);
    if (m_model == nullptr || m_model->context() == nullptr) {
        return TranscriptUpdate{
            .events = {},
            .error = makeRuntimeError(RuntimeErrorCode::InternalRuntimeError,
                                      QStringLiteral("Embedded Whisper model handle is not available")),
        };
    }

    const int result = whisper_full_with_state(m_model->context(),
                                               m_state.get(),
                                               params,
                                               m_bufferedSamples.data(),
                                               static_cast<int>(m_bufferedSamples.size()));
    if (result != 0) {
        m_bufferedSamples.clear();
        if (m_cancelRequested.load(std::memory_order_relaxed)) {
            return makeCancelledUpdate();
        }
        return TranscriptUpdate{
            .events = {},
            .error = makeRuntimeError(RuntimeErrorCode::DecodeFailed,
                                      QStringLiteral("Embedded Whisper transcription failed with code %1").arg(result)),
        };
    }

    TranscriptUpdate update;
    const int segmentCount = whisper_full_n_segments_from_state(m_state.get());
    update.events.reserve(static_cast<std::size_t>(segmentCount));
    for (int index = 0; index < segmentCount; ++index) {
        const char *segment = whisper_full_get_segment_text_from_state(m_state.get(), index);
        const QString text = QString::fromUtf8(segment).trimmed();
        if (text.isEmpty()) {
            continue;
        }
        update.events.push_back(TranscriptEvent{
            .kind = TranscriptEventKind::Final,
            .text = text,
            .startMs = whisper_full_get_segment_t0_from_state(m_state.get(), index) * 10,
            .endMs = whisper_full_get_segment_t1_from_state(m_state.get(), index) * 10,
        });
    }

    m_bufferedSamples.clear();
    qCInfo(whisperCppLog) << "Whisper final segment count:" << update.events.size();

    return update;
}

TranscriptUpdate WhisperCppTranscriber::cancel()
{
    m_cancelRequested.store(true, std::memory_order_relaxed);
    m_bufferedSamples.clear();
    return makeCancelledUpdate();
}

bool WhisperCppTranscriber::ensureState(RuntimeError *error)
{
    if (m_state != nullptr) {
        return true;
    }

    if (m_model == nullptr || m_model->context() == nullptr) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::InternalRuntimeError,
                                      QStringLiteral("Embedded Whisper model handle is not available"));
        }
        return false;
    }

    m_state.reset(whisper_init_state(m_model->context()));
    if (m_state == nullptr) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Failed to initialize embedded Whisper session state: %1")
                                          .arg(m_model->modelPath()));
        }
        return false;
    }

    m_cancelRequested.store(false, std::memory_order_relaxed);
    return true;
}

namespace {

class WhisperCppTranscriptionEngine final : public TranscriptionEngine
{
public:
    explicit WhisperCppTranscriptionEngine(TranscriberConfig config)
        : m_config(std::move(config))
    {
    }

    [[nodiscard]] BackendCapabilities capabilities() const override
    {
        return WhisperCppTranscriber::capabilitiesStatic();
    }

    [[nodiscard]] RuntimeDiagnostics diagnostics() const override
    {
        RuntimeDiagnostics diagnostics = WhisperCppTranscriber::diagnosticsStatic();
        diagnostics.selectionReason = selectRuntimeForConfig(m_config).reason;
        return diagnostics;
    }

    [[nodiscard]] std::shared_ptr<const TranscriptionModelHandle> loadModel(RuntimeError *error) const override
    {
        return WhisperCppTranscriber::loadModelHandle(m_config, error);
    }

    [[nodiscard]] std::unique_ptr<TranscriptionSession>
    createSession(std::shared_ptr<const TranscriptionModelHandle> model) const override
    {
        return WhisperCppTranscriber::createSession(m_config, std::move(model));
    }

private:
    TranscriberConfig m_config;
};

} // namespace

std::shared_ptr<const TranscriptionEngine> createWhisperCppTranscriptionEngine(const TranscriberConfig &config)
{
    return std::make_shared<WhisperCppTranscriptionEngine>(config);
}
