#include "transcription/rawwhisperprobe.h"

#include "transcription/modelpackage.h"

#include <QFile>
#include <QFileInfo>

#include <array>
#include <cstring>
#include <cstdint>
#include <utility>

namespace {

constexpr quint32 kGgmlFileMagic = 0x67676d6cU;
constexpr std::array<const char *, 7> kQuantizationNames{
    "float32",
    "float16",
    "q4_0",
    "q4_1",
    "q5_0",
    "q5_1",
    "q8_0",
};

RuntimeError makeRuntimeError(RuntimeErrorCode code, QString message, QString detail = {})
{
    return RuntimeError{.code = code, .message = std::move(message), .detail = std::move(detail)};
}

template <typename T>
bool readValue(QFile *file, T *value)
{
    if (file == nullptr || value == nullptr) {
        return false;
    }

    const QByteArray bytes = file->read(static_cast<qint64>(sizeof(T)));
    if (bytes.size() != static_cast<qsizetype>(sizeof(T))) {
        return false;
    }

    std::memcpy(value, bytes.constData(), sizeof(T));
    return true;
}

QString modelFamilyFromAudioLayers(int layerCount)
{
    switch (layerCount) {
    case 4:
        return QStringLiteral("tiny");
    case 6:
        return QStringLiteral("base");
    case 12:
        return QStringLiteral("small");
    case 24:
        return QStringLiteral("medium");
    case 32:
        return QStringLiteral("large");
    default:
        return QStringLiteral("unknown");
    }
}

QString languageProfileFromVocabulary(int vocabularySize)
{
    return vocabularySize >= 51865 ? QStringLiteral("multilingual") : QStringLiteral("en");
}

QString quantizationName(int ftype)
{
    if (ftype >= 0 && std::cmp_less(ftype, kQuantizationNames.size())) {
        return QString::fromLatin1(kQuantizationNames.at(static_cast<std::size_t>(ftype)));
    }
    return QStringLiteral("ftype-%1").arg(ftype);
}

} // namespace

std::optional<ModelMetadata> RawWhisperProbe::inspectFile(const QString &path, RuntimeError *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelNotFound,
                                      QStringLiteral("Raw Whisper model not found: %1").arg(path));
        }
        return std::nullopt;
    }

    quint32 magic = 0;
    if (!readValue(&file, &magic) || magic != kGgmlFileMagic) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::InvalidModelPackage,
                                      QStringLiteral("Raw Whisper model has an unsupported or invalid header"),
                                      QFileInfo(path).absoluteFilePath());
        }
        return std::nullopt;
    }

    std::array<qint32, 11> values{};
    for (qint32 &value : values) {
        if (!readValue(&file, &value)) {
            if (error != nullptr) {
                *error = makeRuntimeError(RuntimeErrorCode::InvalidModelPackage,
                                          QStringLiteral("Raw Whisper model header is truncated"),
                                          QFileInfo(path).absoluteFilePath());
            }
            return std::nullopt;
        }
    }

    ModelMetadata metadata;
    metadata.packageId = sanitizePackageId(QFileInfo(path).completeBaseName());
    metadata.displayName = QFileInfo(path).completeBaseName();
    metadata.runtimeFamily = QStringLiteral("asr");
    metadata.sourceFormat = QStringLiteral("whisper.cpp-ggml");
    metadata.modelFormat = QStringLiteral("ggml");
    metadata.architecture = modelFamilyFromAudioLayers(values.at(4));
    metadata.languageProfile = languageProfileFromVocabulary(values.at(0));
    metadata.quantization = quantizationName(values.at(9));
    metadata.tokenizer = QStringLiteral("embedded");
    metadata.legacyCompatibility = true;
    metadata.vocabularySize = values.at(0);
    metadata.audioContext = values.at(1);
    metadata.audioState = values.at(2);
    metadata.audioHeadCount = values.at(3);
    metadata.audioLayerCount = values.at(4);
    metadata.textContext = values.at(5);
    metadata.textState = values.at(6);
    metadata.textHeadCount = values.at(7);
    metadata.textLayerCount = values.at(8);
    metadata.melCount = values.at(9);
    metadata.formatType = values.at(10);
    return metadata;
}
