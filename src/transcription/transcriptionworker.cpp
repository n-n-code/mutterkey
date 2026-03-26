#include "transcription/transcriptionworker.h"

#include <cassert>
#include <utility>

namespace {

RuntimeError makeRuntimeError(RuntimeErrorCode code, QString message)
{
    return RuntimeError{.code = code, .message = std::move(message)};
}

} // namespace

TranscriptionWorker::TranscriptionWorker(std::shared_ptr<const TranscriptionEngine> engine, QObject *parent)
    : QObject(parent)
    , m_engine(std::move(engine))
{
    assert(m_engine != nullptr);
    m_capabilities = m_engine->capabilities();
}

TranscriptionWorker::TranscriptionWorker(std::unique_ptr<TranscriptionSession> transcriber, QObject *parent)
    : QObject(parent)
    , m_transcriber(std::move(transcriber))
{
    assert(m_transcriber != nullptr);
    m_capabilities.backendName = m_transcriber->backendName();
}

QString TranscriptionWorker::backendName() const
{
    return capabilities().backendName;
}

BackendCapabilities TranscriptionWorker::capabilities() const
{
    return m_capabilities;
}

QString TranscriptionWorker::loadedModelDescription() const
{
    if (m_model == nullptr) {
        return {};
    }

    return m_model->modelDescription();
}

bool TranscriptionWorker::warmup(RuntimeError *error)
{
    if (!ensureSession(error)) {
        return false;
    }

    const bool ready = m_transcriber->warmup(error);
    if (!ready && (error == nullptr || shouldDiscardSession(*error))) {
        m_transcriber.reset();
    }

    return ready;
}

void TranscriptionWorker::transcribe(const Recording &recording)
{
    RuntimeError runtimeError;
    if (!ensureSession(&runtimeError)) {
        emit transcriptionFailed(runtimeError);
        return;
    }

    const TranscriptionResult result = m_transcriber->transcribe(recording);
    if (!result.success) {
        if (shouldDiscardSession(result.error)) {
            m_transcriber.reset();
        }
        emit transcriptionFailed(result.error);
        return;
    }

    emit transcriptionReady(result.text);
}

bool TranscriptionWorker::ensureSession(RuntimeError *error)
{
    if (m_transcriber != nullptr) {
        return true;
    }

    if (!ensureModel(error)) {
        return false;
    }

    if (m_engine == nullptr) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::InternalRuntimeError,
                                      QStringLiteral("Transcription engine is not configured"));
        }
        return false;
    }

    m_transcriber = m_engine->createSession(m_model);
    if (m_transcriber == nullptr) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::InternalRuntimeError,
                                      QStringLiteral("Failed to create a transcription session"));
        }
        return false;
    }
    return true;
}

bool TranscriptionWorker::ensureModel(RuntimeError *error)
{
    if (m_model != nullptr) {
        return true;
    }

    if (m_engine == nullptr) {
        return true;
    }

    m_model = m_engine->loadModel(error);
    return m_model != nullptr;
}

bool TranscriptionWorker::shouldDiscardSession(const RuntimeError &error)
{
    switch (error.code) {
    case RuntimeErrorCode::Cancelled:
    case RuntimeErrorCode::ModelLoadFailed:
    case RuntimeErrorCode::DecodeFailed:
    case RuntimeErrorCode::InternalRuntimeError:
        return true;
    case RuntimeErrorCode::None:
    case RuntimeErrorCode::InvalidConfig:
    case RuntimeErrorCode::ModelNotFound:
    case RuntimeErrorCode::AudioNormalizationFailed:
    case RuntimeErrorCode::UnsupportedLanguage:
        return false;
    }

    return true;
}
