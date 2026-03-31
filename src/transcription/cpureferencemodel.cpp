#include "transcription/cpureferencemodel.h"

#include "transcription/cpumodelweights.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include <array>
#include <cmath>
#include <cstring>
#include <utility>

namespace {

constexpr std::array<char, 8> kCpuReferenceMagicV1{'M', 'K', 'C', 'P', 'U', 'R', '1', '\0'};
constexpr std::array<char, 8> kCpuReferenceMagicV2{'M', 'K', 'C', 'P', 'U', 'R', '2', '\0'};
constexpr std::array<char, 8> kCpuReferenceMagicV3{'M', 'K', 'C', 'P', 'U', 'R', '3', '\0'};
constexpr std::uint32_t kCpuReferenceVersion1 = 1;
constexpr std::uint32_t kCpuReferenceVersion2 = 2;
constexpr std::uint32_t kMaximumPhraseCount = 64;
constexpr std::uint32_t kMaximumTextBytes = 2048;
constexpr std::uint32_t kMaximumFeatureBins = 256;

struct CpuReferenceTemplatePhraseHeader {
    std::uint32_t textBytes = 0;
    std::uint32_t featureCount = 0;
};

RuntimeError makeRuntimeError(RuntimeErrorCode code, QString message, QString detail = {})
{
    return RuntimeError{.code = code, .message = std::move(message), .detail = std::move(detail)};
}

bool isBaselineFamilyExecution(const NativeExecutionMetadata &execution)
{
    return execution.executionVersion >= 2 && !execution.baselineFamily.isEmpty();
}

bool readExact(QFile *file, void *destination, std::size_t byteCount)
{
    if (file == nullptr || destination == nullptr || byteCount == 0U) {
        return false;
    }

    const QByteArray bytes = file->read(static_cast<qint64>(byteCount));
    if (bytes.size() != static_cast<qsizetype>(byteCount)) {
        return false;
    }

    std::memcpy(destination, bytes.constData(), byteCount);
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
    if (!readExact(&file, &header, sizeof(CpuReferenceModelHeader))) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Native CPU model header is truncated"),
                                      QFileInfo(path).absoluteFilePath());
        }
        return std::nullopt;
    }

    if (header.magic == kCpuReferenceMagicV1) {
        if (header.version != kCpuReferenceVersion1) {
            if (error != nullptr) {
                *error = makeRuntimeError(RuntimeErrorCode::UnsupportedModelPackageVersion,
                                          QStringLiteral("Native CPU fixture model version is unsupported"),
                                          QString::number(header.version));
            }
            return std::nullopt;
        }

        const std::uint32_t transcriptBytes = header.payloadField1;
        if (transcriptBytes == 0U || transcriptBytes > 4096U) {
            if (error != nullptr) {
                *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                          QStringLiteral("Native CPU fixture transcript payload is invalid"),
                                          QString::number(transcriptBytes));
            }
            return std::nullopt;
        }

        const QByteArray transcriptBytesUtf8 = file.read(static_cast<qint64>(transcriptBytes));
        if (transcriptBytesUtf8.size() != static_cast<qsizetype>(transcriptBytes)) {
            if (error != nullptr) {
                *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                          QStringLiteral("Native CPU fixture transcript payload is truncated"),
                                          QFileInfo(path).absoluteFilePath());
            }
            return std::nullopt;
        }

        const QString transcript = QString::fromUtf8(transcriptBytesUtf8).trimmed();
        if (transcript.isEmpty()) {
            if (error != nullptr) {
                *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                          QStringLiteral("Native CPU fixture transcript payload is empty"),
                                          QFileInfo(path).absoluteFilePath());
            }
            return std::nullopt;
        }

        return CpuReferenceModelData{
            .kind = CpuReferenceModelKind::FixtureV1,
            .transcript = transcript,
        };
    }

    if (header.magic == kCpuReferenceMagicV3) {
        // V3 is a real tensor-backed model. The tensor weights are loaded
        // separately by loadCpuWhisperModelWeights(). Return a minimal
        // RealDecoderV3 model data here; the tensor payload is attached later.
        return CpuReferenceModelData{
            .kind = CpuReferenceModelKind::RealDecoderV3,
        };
    }

    if (header.magic != kCpuReferenceMagicV2) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Native CPU model has an invalid header"),
                                      QFileInfo(path).absoluteFilePath());
        }
        return std::nullopt;
    }

    if (header.version != kCpuReferenceVersion2) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::UnsupportedModelPackageVersion,
                                      QStringLiteral("Native CPU decoder model version is unsupported"),
                                      QString::number(header.version));
        }
        return std::nullopt;
    }

    const std::uint32_t featureBinCount = header.payloadField1;
    const std::uint32_t phraseCount = header.payloadField2;
    const float maxDistance = static_cast<float>(header.payloadField3) / 1000.0F;
    if (featureBinCount == 0U || featureBinCount > kMaximumFeatureBins) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Native CPU decoder feature-bin count is invalid"),
                                      QString::number(featureBinCount));
        }
        return std::nullopt;
    }
    if (phraseCount == 0U || phraseCount > kMaximumPhraseCount) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Native CPU decoder phrase count is invalid"),
                                      QString::number(phraseCount));
        }
        return std::nullopt;
    }
    if (maxDistance <= 0.0F || maxDistance > 4.0F) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Native CPU decoder match threshold is invalid"),
                                      QString::number(maxDistance, 'f', 3));
        }
        return std::nullopt;
    }

    CpuReferenceModelData model{
        .kind = CpuReferenceModelKind::TemplateDecoderScaffoldV2,
        .transcript = {},
        .featureBinCount = static_cast<int>(featureBinCount),
        .maxDistance = maxDistance,
        .phraseTemplates = {},
    };
    model.phraseTemplates.reserve(static_cast<std::size_t>(phraseCount));

    for (std::uint32_t phraseIndex = 0; phraseIndex < phraseCount; ++phraseIndex) {
        CpuReferenceTemplatePhraseHeader phraseHeader;
        if (!readExact(&file, &phraseHeader, sizeof(CpuReferenceTemplatePhraseHeader))) {
            if (error != nullptr) {
                *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                          QStringLiteral("Native CPU decoder phrase header is truncated"),
                                          QFileInfo(path).absoluteFilePath());
            }
            return std::nullopt;
        }

        if (phraseHeader.textBytes == 0U || phraseHeader.textBytes > kMaximumTextBytes
            || phraseHeader.featureCount != featureBinCount) {
            if (error != nullptr) {
                *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                          QStringLiteral("Native CPU decoder phrase metadata is invalid"),
                                          QStringLiteral("phrase=%1").arg(phraseIndex));
            }
            return std::nullopt;
        }

        const QByteArray textBytes = file.read(static_cast<qint64>(phraseHeader.textBytes));
        if (textBytes.size() != static_cast<qsizetype>(phraseHeader.textBytes)) {
            if (error != nullptr) {
                *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                          QStringLiteral("Native CPU decoder phrase text is truncated"),
                                          QStringLiteral("phrase=%1").arg(phraseIndex));
            }
            return std::nullopt;
        }

        const QString text = QString::fromUtf8(textBytes).trimmed();
        if (text.isEmpty()) {
            if (error != nullptr) {
                *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                          QStringLiteral("Native CPU decoder phrase text is empty"),
                                          QStringLiteral("phrase=%1").arg(phraseIndex));
            }
            return std::nullopt;
        }

        std::vector<float> featureProfile(static_cast<std::size_t>(featureBinCount), 0.0F);
        if (!readExact(&file, featureProfile.data(), featureProfile.size() * sizeof(float))) {
            if (error != nullptr) {
                *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                          QStringLiteral("Native CPU decoder phrase feature profile is truncated"),
                                          QStringLiteral("phrase=%1").arg(phraseIndex));
            }
            return std::nullopt;
        }

        model.phraseTemplates.push_back(CpuDecodedPhraseTemplate{
            .text = text,
            .featureProfile = std::move(featureProfile),
        });
    }

    return model;
}

std::optional<std::vector<QString>> loadTokenizerVocabulary(const ValidatedModelPackage &package, RuntimeError *error)
{
    const QString role = package.nativeExecution().tokenizerAssetRole;
    if (role.isEmpty()) {
        return std::vector<QString>{};
    }

    const std::optional<QString> path = package.resolvedAssetPath(role);
    if (!path.has_value()) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::InvalidModelPackage,
                                      QStringLiteral("Native CPU decoder tokenizer asset is missing"),
                                      role);
        }
        return std::nullopt;
    }

    QFile file(*path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Failed to open native CPU tokenizer asset"),
                                      *path);
        }
        return std::nullopt;
    }

    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    std::vector<QString> vocabulary;
    vocabulary.reserve(static_cast<std::size_t>(lines.size()));
    for (const QString &line : lines) {
        QString token = line;
        if (token.endsWith(QLatin1Char('\r'))) {
            token.chop(1);
        }
        if (!token.isEmpty()) {
            vocabulary.push_back(token);
        }
    }

    if (package.metadata().vocabularySize > 0
        && std::cmp_not_equal(package.metadata().vocabularySize, vocabulary.size())) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::InvalidModelPackage,
                                      QStringLiteral("Native CPU tokenizer vocabulary size does not match package metadata"),
                                      *path);
        }
        return std::nullopt;
    }

    return vocabulary;
}

std::optional<CpuWhisperTokenizerModel> loadWhisperTokenizer(const ValidatedModelPackage &package, RuntimeError *error)
{
    const NativeExecutionMetadata &execution = package.nativeExecution();
    if (execution.executionVersion < 2 || execution.tokenizerMergesAssetRole.isEmpty()) {
        return std::nullopt;
    }

    const std::optional<QString> vocabularyPath = package.resolvedAssetPath(execution.tokenizerAssetRole);
    const std::optional<QString> mergesPath = package.resolvedAssetPath(execution.tokenizerMergesAssetRole);
    if (!vocabularyPath.has_value() || !mergesPath.has_value()) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::InvalidModelPackage,
                                      QStringLiteral("Native CPU decoder tokenizer assets are incomplete"),
                                      package.packageRootPath);
        }
        return std::nullopt;
    }

    return loadCpuWhisperTokenizerModel(*vocabularyPath, *mergesPath, error);
}

} // namespace

CpuReferenceModelHandle::CpuReferenceModelHandle(ValidatedModelPackage package, CpuReferenceModelData model, qint64 loadTimeMs)
    : m_package(std::move(package))
    , m_model(std::move(model))
    , m_execution(CpuReferenceExecutionMetadata{
          .decoder = m_package.nativeExecution().decoder,
          .tokenizer = m_package.nativeExecution().tokenizer,
          .baselineFamily = m_package.nativeExecution().baselineFamily,
          .tokenizerAssetRole = m_package.nativeExecution().tokenizerAssetRole,
          .tokenizerMergesAssetRole = m_package.nativeExecution().tokenizerMergesAssetRole,
          .frontend = m_package.nativeExecution().frontend,
          .searchPolicy = m_package.nativeExecution().searchPolicy,
          .timestampMode = m_package.nativeExecution().timestampMode,
          .featureBinCount = m_package.nativeExecution().featureBinCount,
          .templateCount = m_package.nativeExecution().templateCount,
          .maxDistance = m_package.nativeExecution().maxDistance,
          .bosTokenId = m_package.nativeExecution().bosTokenId,
          .eosTokenId = m_package.nativeExecution().eosTokenId,
          .noSpeechTokenId = m_package.nativeExecution().noSpeechTokenId,
          .timestampTokenStartId = m_package.nativeExecution().timestampTokenStartId,
          .timestampTokenEndId = m_package.nativeExecution().timestampTokenEndId,
      })
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
    QString shapeDescription;
    if (m_model.kind == CpuReferenceModelKind::RealDecoderV3) {
        shapeDescription = QStringLiteral("real-decoder family=%1 tokenizer=%2")
                               .arg(m_execution.baselineFamily, m_execution.tokenizer);
    } else if (m_model.kind != CpuReferenceModelKind::FixtureV1) {
        const QString modelKind =
            m_model.kind == CpuReferenceModelKind::BaselineFamilyDecoderV2 ? QStringLiteral("baseline-decoder")
                                                                           : QStringLiteral("template-scaffold");
        shapeDescription = QStringLiteral("%1 family=%2 kind=%3 phrases=%4 bins=%5 threshold=%6 tokenizer=%7")
                               .arg(m_execution.decoder,
                                    m_execution.baselineFamily,
                                    modelKind,
                                    QString::number(m_execution.templateCount),
                                    QString::number(m_execution.featureBinCount),
                                    QString::number(m_execution.maxDistance, 'f', 3),
                                    m_execution.tokenizer);
    } else {
        shapeDescription = QStringLiteral("fixture transcript");
    }
    return QStringLiteral("%1; %2; load=%3 ms").arg(m_package.description(), shapeDescription).arg(m_loadTimeMs);
}

const CpuReferenceModelData &CpuReferenceModelHandle::model() const
{
    return m_model;
}

const CpuReferenceExecutionMetadata &CpuReferenceModelHandle::execution() const
{
    return m_execution;
}

std::shared_ptr<const CpuReferenceModelHandle> loadCpuReferenceModelHandle(const ValidatedModelPackage &package, RuntimeError *error)
{
    QElapsedTimer timer;
    timer.start();
    const std::optional<CpuReferenceModelData> model = loadCpuReferenceModelData(package.weightsPath, error);
    if (!model.has_value()) {
        return nullptr;
    }
    CpuReferenceModelData resolvedModel = *model;
    const std::optional<std::vector<QString>> vocabulary = loadTokenizerVocabulary(package, error);
    if (!vocabulary.has_value()) {
        return nullptr;
    }
    resolvedModel.tokenVocabulary = *vocabulary;
    if (const std::optional<CpuWhisperTokenizerModel> whisperTokenizer = loadWhisperTokenizer(package, error);
        whisperTokenizer.has_value()) {
        resolvedModel.whisperTokenizer = *whisperTokenizer;
    } else if (error != nullptr && !error->isOk()) {
        return nullptr;
    }
    const NativeExecutionMetadata &execution = package.nativeExecution();
    if (resolvedModel.kind == CpuReferenceModelKind::RealDecoderV3) {
        std::shared_ptr<CpuWhisperModelWeights> tensorWeights =
            loadCpuWhisperModelWeights(package.weightsPath, error);
        if (tensorWeights == nullptr) {
            return nullptr;
        }
        resolvedModel.tensorWeights = std::move(tensorWeights);
        return std::make_shared<CpuReferenceModelHandle>(package, std::move(resolvedModel), timer.elapsed());
    }
    if (resolvedModel.kind == CpuReferenceModelKind::TemplateDecoderScaffoldV2
        || resolvedModel.kind == CpuReferenceModelKind::BaselineFamilyDecoderV2) {
        if (execution.executionVersion <= 1
            && (execution.featureBinCount != resolvedModel.featureBinCount
                || std::cmp_not_equal(execution.templateCount, resolvedModel.phraseTemplates.size())
                || std::abs(execution.maxDistance - static_cast<double>(resolvedModel.maxDistance)) > 0.0005)) {
            if (error != nullptr) {
                *error = makeRuntimeError(RuntimeErrorCode::InvalidModelPackage,
                                          QStringLiteral("Native CPU decoder package metadata does not match the weights payload"),
                                          package.weightsPath);
            }
            return nullptr;
        }
        if (isBaselineFamilyExecution(execution)) {
            resolvedModel.kind = CpuReferenceModelKind::BaselineFamilyDecoderV2;
            if (!resolvedModel.whisperTokenizer.has_value()) {
                if (error != nullptr) {
                    *error = makeRuntimeError(RuntimeErrorCode::InvalidModelPackage,
                                              QStringLiteral("Baseline-family native decoder package is missing a loaded tokenizer model"),
                                              package.packageRootPath);
                }
                return nullptr;
            }
        }
    }

    return std::make_shared<CpuReferenceModelHandle>(package, std::move(resolvedModel), timer.elapsed());
}

std::shared_ptr<const CpuReferenceModelHandle>
resolveCpuReferenceModelHandle(std::shared_ptr<const TranscriptionModelHandle> model)
{
    return std::dynamic_pointer_cast<const CpuReferenceModelHandle>(std::move(model));
}
