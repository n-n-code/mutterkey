#include "transcription/cpureferencemodel.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <array>
#include <cstring>

namespace {

constexpr std::array<char, 8> kCpuReferenceMagic{'M', 'K', 'C', 'P', 'U', 'R', '1', '\0'};
constexpr std::uint32_t kCpuReferenceVersion = 1;

RuntimeError makeRuntimeError(RuntimeErrorCode code, QString message, QString detail = {})
{
    return RuntimeError{.code = code, .message = std::move(message), .detail = std::move(detail)};
}

bool readModelHeader(QFile *file, CpuReferenceModelHeader *header)
{
    if (file == nullptr || header == nullptr) {
        return false;
    }

    const QByteArray bytes = file->read(static_cast<qint64>(sizeof(CpuReferenceModelHeader)));
    if (bytes.size() != static_cast<qsizetype>(sizeof(CpuReferenceModelHeader))) {
        return false;
    }

    std::memcpy(header, bytes.constData(), sizeof(CpuReferenceModelHeader));
    return true;
}

std::optional<CpuReferenceModelData> loadCpuReferenceModelData(const QString &path, RuntimeError *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Failed to open native CPU model weights"),
                                      QFileInfo(path).absoluteFilePath());
        }
        return std::nullopt;
    }

    CpuReferenceModelHeader header;
    if (!readModelHeader(&file, &header)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Native CPU model header is truncated"),
                                      QFileInfo(path).absoluteFilePath());
        }
        return std::nullopt;
    }

    if (header.magic != kCpuReferenceMagic) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Native CPU model has an invalid header"),
                                      QFileInfo(path).absoluteFilePath());
        }
        return std::nullopt;
    }

    if (header.version != kCpuReferenceVersion) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::UnsupportedModelPackageVersion,
                                      QStringLiteral("Native CPU model version is unsupported"),
                                      QString::number(header.version));
        }
        return std::nullopt;
    }

    if (header.transcriptBytes == 0 || header.transcriptBytes > 4096U) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Native CPU model transcript payload is invalid"),
                                      QString::number(header.transcriptBytes));
        }
        return std::nullopt;
    }

    const QByteArray transcriptBytes = file.read(static_cast<qint64>(header.transcriptBytes));
    if (transcriptBytes.size() != static_cast<qsizetype>(header.transcriptBytes)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Native CPU model transcript payload is truncated"),
                                      QFileInfo(path).absoluteFilePath());
        }
        return std::nullopt;
    }

    const QString transcript = QString::fromUtf8(transcriptBytes).trimmed();
    if (transcript.isEmpty()) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Native CPU model transcript payload is empty"),
                                      QFileInfo(path).absoluteFilePath());
        }
        return std::nullopt;
    }

    return CpuReferenceModelData{.transcript = transcript};
}

} // namespace

CpuReferenceModelHandle::CpuReferenceModelHandle(ValidatedModelPackage package, CpuReferenceModelData model, qint64 loadTimeMs)
    : m_package(std::move(package))
    , m_model(std::move(model))
    , m_loadTimeMs(loadTimeMs)
{
}

QString CpuReferenceModelHandle::backendName() const
{
    return cpuReferenceEngineName();
}

ModelMetadata CpuReferenceModelHandle::metadata() const
{
    return m_package.metadata();
}

QString CpuReferenceModelHandle::modelDescription() const
{
    return QStringLiteral("%1; load=%2 ms").arg(m_package.description()).arg(m_loadTimeMs);
}

const QString &CpuReferenceModelHandle::transcript() const
{
    return m_model.transcript;
}

std::shared_ptr<const CpuReferenceModelHandle> loadCpuReferenceModelHandle(const ValidatedModelPackage &package, RuntimeError *error)
{
    QElapsedTimer timer;
    timer.start();
    const std::optional<CpuReferenceModelData> model = loadCpuReferenceModelData(package.weightsPath, error);
    if (!model.has_value()) {
        return nullptr;
    }

    return std::make_shared<CpuReferenceModelHandle>(package, *model, timer.elapsed());
}

std::shared_ptr<const CpuReferenceModelHandle>
resolveCpuReferenceModelHandle(std::shared_ptr<const TranscriptionModelHandle> model)
{
    return std::dynamic_pointer_cast<const CpuReferenceModelHandle>(std::move(model));
}
