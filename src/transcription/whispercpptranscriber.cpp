#include "transcription/whispercpptranscriber.h"

#include <algorithm>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStringList>
#include <QThread>
#include <utility>

extern "C" {
#include <ggml-backend.h>
#include <whisper.h>
}

namespace {

Q_STATIC_LOGGING_CATEGORY(whisperCppLog, "mutterkey.transcriber.whispercpp")

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

} // namespace

WhisperCppTranscriber::WhisperCppTranscriber(TranscriberConfig config)
    : m_config(std::move(config))
    , m_context(nullptr, &WhisperCppTranscriber::freeContext)
{
}

WhisperCppTranscriber::~WhisperCppTranscriber() = default;

void WhisperCppTranscriber::freeContext(whisper_context *context) noexcept
{
    if (context != nullptr) {
        whisper_free(context);
    }
}

QString WhisperCppTranscriber::backendName()
{
    return QStringLiteral("whisper.cpp");
}

bool WhisperCppTranscriber::warmup(QString *errorMessage)
{
    return ensureInitialized(errorMessage);
}

TranscriptionResult WhisperCppTranscriber::transcribe(const Recording &recording)
{
    QString errorMessage;
    if (!ensureInitialized(&errorMessage)) {
        return TranscriptionResult{.success = false, .text = {}, .error = errorMessage};
    }

    NormalizedAudio normalizedAudio;
    if (!m_normalizer.normalizeForWhisper(recording, &normalizedAudio, &errorMessage)) {
        return TranscriptionResult{.success = false, .text = {}, .error = errorMessage};
    }

    qCInfo(whisperCppLog) << "Normalized recording to"
                          << normalizedAudio.samples.size()
                          << "mono samples at"
                          << normalizedAudio.sampleRate
                          << "Hz";

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

    const QByteArray language = m_config.language.trimmed().toUtf8();
    if (!language.isEmpty()) {
        params.language = language.constData();
    }

    const int result = whisper_full(m_context.get(),
                                    params,
                                    normalizedAudio.samples.data(),
                                    static_cast<int>(normalizedAudio.samples.size()));
    if (result != 0) {
        return TranscriptionResult{
            .success = false,
            .text = {},
            .error = QStringLiteral("Embedded Whisper transcription failed with code %1").arg(result),
        };
    }

    // whisper.cpp returns text in segment chunks; join non-empty segments into the single
    // clipboard-friendly transcript expected by the rest of the app.
    QString transcript;
    const int segmentCount = whisper_full_n_segments(m_context.get());
    for (int index = 0; index < segmentCount; ++index) {
        const char *segment = whisper_full_get_segment_text(m_context.get(), index);
        const QString text = QString::fromUtf8(segment).trimmed();
        if (text.isEmpty()) {
            continue;
        }
        if (!transcript.isEmpty()) {
            transcript += QLatin1Char(' ');
        }
        transcript += text;
    }

    qCInfo(whisperCppLog) << "Whisper transcript length:" << transcript.trimmed().size();

    return TranscriptionResult{.success = true, .text = transcript.trimmed(), .error = {}};
}

bool WhisperCppTranscriber::ensureInitialized(QString *errorMessage)
{
    if (m_context != nullptr) {
        return true;
    }

    // Resolve to an absolute path before initialization so logs and error messages match
    // the exact model file whisper.cpp is attempting to load.
    const QString modelPath = QFileInfo(m_config.modelPath).absoluteFilePath();
    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Embedded Whisper model not found: %1").arg(m_config.modelPath);
        }
        return false;
    }

    const whisper_context_params contextParams = whisper_context_default_params();
    qCInfo(whisperCppLog).noquote() << "ggml runtime:" << describeRegisteredBackends();
    m_context.reset(whisper_init_from_file_with_params(modelPath.toUtf8().constData(), contextParams));
    if (m_context == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to load embedded Whisper model: %1").arg(modelPath);
        }
        return false;
    }

    qCInfo(whisperCppLog) << "Loaded embedded Whisper model from" << modelPath;
    return true;
}
