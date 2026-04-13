#include "asr/nativecpu/cpumodelweights.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <utility>

namespace {

constexpr std::array<char, 8> kMkcpur3Magic{'M', 'K', 'C', 'P', 'U', 'R', '3', '\0'};
constexpr std::uint32_t kMkcpur3Version = 3;
constexpr int kDtypeF32 = 0;
constexpr int kDtypeF16 = 1;

RuntimeError makeRuntimeError(RuntimeErrorCode code, QString message, QString detail = {})
{
    return RuntimeError{.code = code, .message = std::move(message), .detail = std::move(detail)};
}

bool readExact(QFile *file, void *destination, std::size_t byteCount)
{
    if (file == nullptr || destination == nullptr || byteCount == 0U) {
        return false;
    }
    const qint64 bytesRead = file->read(static_cast<char *>(destination), static_cast<qint64>(byteCount));
    return std::cmp_equal(bytesRead, byteCount);
}

/**
 * @brief Converts a half-precision float to single-precision.
 */
float halfToFloat(std::uint16_t h)
{
    const std::uint32_t sign = (static_cast<std::uint32_t>(h) & 0x8000U) << 16U;
    std::uint32_t exponent = (static_cast<std::uint32_t>(h) >> 10U) & 0x1FU;
    std::uint32_t mantissa = static_cast<std::uint32_t>(h) & 0x03FFU;

    if (exponent == 0U) {
        if (mantissa == 0U) {
            // Zero.
            std::uint32_t bits = sign;
            float result = 0.0F;
            std::memcpy(&result, &bits, sizeof(float));
            return result;
        }
        // Subnormal: normalize.
        while ((mantissa & 0x0400U) == 0U) {
            mantissa <<= 1U;
            exponent -= 1U;
        }
        exponent += 1U;
        mantissa &= ~0x0400U;
    } else if (exponent == 31U) {
        // Inf or NaN.
        const std::uint32_t bits = sign | 0x7F800000U | (mantissa << 13U);
        float result = 0.0F;
        std::memcpy(&result, &bits, sizeof(float));
        return result;
    }

    exponent = exponent + 127U - 15U;
    const std::uint32_t bits = sign | (exponent << 23U) | (mantissa << 13U);
    float result = 0.0F;
    std::memcpy(&result, &bits, sizeof(float));
    return result;
}

/**
 * @brief Reads a tensor data block, converting f16 to f32 if needed.
 */
bool readTensorData(QFile *file,
                    CpuTensor *tensor,
                    const CpuTensorDirectoryEntry &entry)
{
    const int elementCount = tensor->elementCount();
    if (entry.dtype == kDtypeF32) {
        const std::size_t expectedSize = static_cast<std::size_t>(elementCount) * sizeof(float);
        if (std::cmp_not_equal(entry.dataSize, expectedSize)) {
            return false;
        }
        return readExact(file, tensor->data.data(), expectedSize);
    }

    if (entry.dtype == kDtypeF16) {
        const auto halfCount = static_cast<std::size_t>(elementCount);
        const std::size_t expectedSize = halfCount * sizeof(std::uint16_t);
        if (std::cmp_not_equal(entry.dataSize, expectedSize)) {
            return false;
        }
        std::vector<std::uint16_t> halfData(halfCount);
        if (!readExact(file, halfData.data(), expectedSize)) {
            return false;
        }
        for (std::size_t i = 0; i < halfCount; ++i) {
            tensor->data.at(i) = halfToFloat(halfData.at(i));
        }
        return true;
    }

    return false;
}

bool readTensorDirectory(QFile *file,
                         std::vector<CpuTensorDirectoryEntry> *directory,
                         std::uint32_t tensorCount)
{
    directory->resize(tensorCount);
    for (std::uint32_t i = 0; i < tensorCount; ++i) {
        CpuTensorDirectoryEntry &entry = directory->at(i);

        std::uint32_t nameLength = 0;
        if (!readExact(file, &nameLength, sizeof(nameLength))) {
            return false;
        }
        if (nameLength == 0 || nameLength > 256) {
            return false;
        }

        QByteArray nameBytes(static_cast<qsizetype>(nameLength), Qt::Uninitialized);
        if (!readExact(file, nameBytes.data(), nameLength)) {
            return false;
        }
        entry.name = QString::fromUtf8(nameBytes);

        std::uint32_t nDims = 0;
        if (!readExact(file, &nDims, sizeof(nDims))) {
            return false;
        }
        if (nDims == 0 || nDims > 3) {
            return false;
        }
        entry.nDims = static_cast<int>(nDims);
        entry.dims.resize(nDims);
        for (std::uint32_t d = 0; d < nDims; ++d) {
            std::uint32_t dimSize = 0;
            if (!readExact(file, &dimSize, sizeof(dimSize))) {
                return false;
            }
            entry.dims.at(d) = static_cast<int>(dimSize);
        }

        std::uint32_t dtype = 0;
        if (!readExact(file, &dtype, sizeof(dtype))) {
            return false;
        }
        entry.dtype = static_cast<int>(dtype);

        std::int64_t dataOffset = 0;
        std::int64_t dataSize = 0;
        if (!readExact(file, &dataOffset, sizeof(dataOffset))) {
            return false;
        }
        if (!readExact(file, &dataSize, sizeof(dataSize))) {
            return false;
        }
        entry.dataOffset = dataOffset;
        entry.dataSize = dataSize;
    }
    return true;
}

const CpuTensorDirectoryEntry *findEntry(const std::vector<CpuTensorDirectoryEntry> &directory,
                                          QStringView name)
{
    for (const CpuTensorDirectoryEntry &entry : directory) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

/**
 * @brief Loads one tensor by name from the data section.
 */
bool loadNamedTensor(QFile *file,
                     const std::vector<CpuTensorDirectoryEntry> &directory,
                     std::int64_t dataSectionOffset,
                     QStringView name,
                     CpuTensor *tensor)
{
    const CpuTensorDirectoryEntry *entry = findEntry(directory, name);
    if (entry == nullptr) {
        return false;
    }

    int rows = 1;
    int cols = 1;
    if (entry->nDims == 1) {
        cols = entry->dims.at(0);
    } else if (entry->nDims >= 2) {
        rows = entry->dims.at(0);
        cols = entry->dims.at(1);
    }
    *tensor = CpuTensor(rows, cols);

    file->seek(dataSectionOffset + entry->dataOffset);
    return readTensorData(file, tensor, *entry);
}

struct CpuLayerLoadRequest {
    QFile *file = nullptr;
    const std::vector<CpuTensorDirectoryEntry> &directory;
    std::int64_t dataSectionOffset = 0;
    int layerIndex = 0;
};

bool loadEncoderLayerWeights(const CpuLayerLoadRequest &request, CpuEncoderLayerWeights *layer)
{
    const QString prefix = QStringLiteral("encoder.blocks.%1.").arg(request.layerIndex);

    auto load = [&](QStringView suffix, CpuTensor *t) {
        return loadNamedTensor(request.file, request.directory, request.dataSectionOffset, prefix + suffix, t);
    };

    return load(u"attn_ln.weight", &layer->attnLnGamma)
           && load(u"attn_ln.bias", &layer->attnLnBeta)
           && load(u"attn.query.weight", &layer->queryWeight)
           && load(u"attn.query.bias", &layer->queryBias)
           && load(u"attn.key.weight", &layer->keyWeight)
           && load(u"attn.value.weight", &layer->valueWeight)
           && load(u"attn.value.bias", &layer->valueBias)
           && load(u"attn.out.weight", &layer->attnOutWeight)
           && load(u"attn.out.bias", &layer->attnOutBias)
           && load(u"mlp_ln.weight", &layer->ffnLnGamma)
           && load(u"mlp_ln.bias", &layer->ffnLnBeta)
           && load(u"mlp.0.weight", &layer->ffn1Weight)
           && load(u"mlp.0.bias", &layer->ffn1Bias)
           && load(u"mlp.2.weight", &layer->ffn2Weight)
           && load(u"mlp.2.bias", &layer->ffn2Bias);
}

bool loadDecoderLayerWeights(const CpuLayerLoadRequest &request, CpuDecoderLayerWeights *layer)
{
    const QString prefix = QStringLiteral("decoder.blocks.%1.").arg(request.layerIndex);

    auto load = [&](QStringView suffix, CpuTensor *t) {
        return loadNamedTensor(request.file, request.directory, request.dataSectionOffset, prefix + suffix, t);
    };

    return load(u"attn_ln.weight", &layer->selfAttnLnGamma)
           && load(u"attn_ln.bias", &layer->selfAttnLnBeta)
           && load(u"attn.query.weight", &layer->selfQueryWeight)
           && load(u"attn.query.bias", &layer->selfQueryBias)
           && load(u"attn.key.weight", &layer->selfKeyWeight)
           && load(u"attn.value.weight", &layer->selfValueWeight)
           && load(u"attn.value.bias", &layer->selfValueBias)
           && load(u"attn.out.weight", &layer->selfAttnOutWeight)
           && load(u"attn.out.bias", &layer->selfAttnOutBias)
           && load(u"cross_attn_ln.weight", &layer->crossAttnLnGamma)
           && load(u"cross_attn_ln.bias", &layer->crossAttnLnBeta)
           && load(u"cross_attn.query.weight", &layer->crossQueryWeight)
           && load(u"cross_attn.query.bias", &layer->crossQueryBias)
           && load(u"cross_attn.key.weight", &layer->crossKeyWeight)
           && load(u"cross_attn.value.weight", &layer->crossValueWeight)
           && load(u"cross_attn.value.bias", &layer->crossValueBias)
           && load(u"cross_attn.out.weight", &layer->crossAttnOutWeight)
           && load(u"cross_attn.out.bias", &layer->crossAttnOutBias)
           && load(u"mlp_ln.weight", &layer->ffnLnGamma)
           && load(u"mlp_ln.bias", &layer->ffnLnBeta)
           && load(u"mlp.0.weight", &layer->ffn1Weight)
           && load(u"mlp.0.bias", &layer->ffn1Bias)
           && load(u"mlp.2.weight", &layer->ffn2Weight)
           && load(u"mlp.2.bias", &layer->ffn2Bias);
}

CpuModelConfig parseModelConfig(const QJsonObject &metadata)
{
    CpuModelConfig config;
    config.melBands = metadata.value(QStringLiteral("n_mels")).toInt(80);
    config.audioContextSize = metadata.value(QStringLiteral("n_audio_ctx")).toInt(1500);
    config.audioStateSize = metadata.value(QStringLiteral("n_audio_state")).toInt(512);
    config.audioHeadCount = metadata.value(QStringLiteral("n_audio_head")).toInt(8);
    config.audioLayerCount = metadata.value(QStringLiteral("n_audio_layer")).toInt(6);
    config.textContextSize = metadata.value(QStringLiteral("n_text_ctx")).toInt(448);
    config.textStateSize = metadata.value(QStringLiteral("n_text_state")).toInt(512);
    config.textHeadCount = metadata.value(QStringLiteral("n_text_head")).toInt(8);
    config.textLayerCount = metadata.value(QStringLiteral("n_text_layer")).toInt(6);
    config.vocabularySize = metadata.value(QStringLiteral("n_vocab")).toInt(51864);
    return config;
}

bool loadMelFilters(QFile *file,
                    const std::vector<CpuTensorDirectoryEntry> &directory,
                    std::int64_t dataSectionOffset,
                    CpuMelFilterBank *filterBank,
                    const CpuModelConfig &config)
{
    const CpuTensorDirectoryEntry *entry = findEntry(directory, u"mel_filters");
    if (entry == nullptr) {
        // No mel filters in model - compute them.
        *filterBank = computeMelFilterBank(CpuMelFilterBankSpec{
            .sampleRate = 16000,
            .fftSize = 400,
            .melBands = config.melBands,
        });
        return true;
    }

    const int fftBins = (400 / 2) + 1;
    filterBank->melBands = config.melBands;
    filterBank->fftBins = fftBins;
    filterBank->filters.resize(static_cast<std::size_t>(config.melBands) * fftBins);

    CpuTensor melTensor(config.melBands, fftBins);
    file->seek(dataSectionOffset + entry->dataOffset);
    if (!readTensorData(file, &melTensor, *entry)) {
        return false;
    }
    filterBank->filters = std::move(melTensor.data);
    return true;
}

bool expectShape(const CpuTensor &tensor, int expectedRows, int expectedCols, const char *name,
                 QString *mismatch)
{
    if (tensor.rows == expectedRows && tensor.cols == expectedCols) {
        return true;
    }
    *mismatch = QStringLiteral("Tensor %1: expected (%2, %3), got (%4, %5)")
                    .arg(QLatin1String(name))
                    .arg(expectedRows)
                    .arg(expectedCols)
                    .arg(tensor.rows)
                    .arg(tensor.cols);
    return false;
}

struct LayerDimensionValidationRequest {
    int stateSize = 0;
    int layerIndex = 0;
    QString *mismatch = nullptr;
};

bool validateEncoderLayerDimensions(const CpuEncoderLayerWeights &layer,
                                    const LayerDimensionValidationRequest &request)
{
    const int ffnSize = 4 * request.stateSize;
    const auto pfx = QStringLiteral("encoder.blocks.%1.").arg(request.layerIndex);

#define CHECK(tensor, r, c, suffix) \
    if (!expectShape(layer.tensor, r, c, qPrintable(pfx + QStringLiteral(suffix)), request.mismatch)) return false

    CHECK(attnLnGamma, 1, request.stateSize, "attn_ln.weight");
    CHECK(attnLnBeta, 1, request.stateSize, "attn_ln.bias");
    CHECK(queryWeight, request.stateSize, request.stateSize, "attn.query.weight");
    CHECK(queryBias, 1, request.stateSize, "attn.query.bias");
    CHECK(keyWeight, request.stateSize, request.stateSize, "attn.key.weight");
    CHECK(valueWeight, request.stateSize, request.stateSize, "attn.value.weight");
    CHECK(valueBias, 1, request.stateSize, "attn.value.bias");
    CHECK(attnOutWeight, request.stateSize, request.stateSize, "attn.out.weight");
    CHECK(attnOutBias, 1, request.stateSize, "attn.out.bias");
    CHECK(ffnLnGamma, 1, request.stateSize, "mlp_ln.weight");
    CHECK(ffnLnBeta, 1, request.stateSize, "mlp_ln.bias");
    CHECK(ffn1Weight, ffnSize, request.stateSize, "mlp.0.weight");
    CHECK(ffn1Bias, 1, ffnSize, "mlp.0.bias");
    CHECK(ffn2Weight, request.stateSize, ffnSize, "mlp.2.weight");
    CHECK(ffn2Bias, 1, request.stateSize, "mlp.2.bias");

#undef CHECK
    return true;
}

bool validateDecoderLayerDimensions(const CpuDecoderLayerWeights &layer,
                                    const LayerDimensionValidationRequest &request)
{
    const int ffnSize = 4 * request.stateSize;
    const auto pfx = QStringLiteral("decoder.blocks.%1.").arg(request.layerIndex);

#define CHECK(tensor, r, c, suffix) \
    if (!expectShape(layer.tensor, r, c, qPrintable(pfx + QStringLiteral(suffix)), request.mismatch)) return false

    // Self-attention.
    CHECK(selfAttnLnGamma, 1, request.stateSize, "attn_ln.weight");
    CHECK(selfAttnLnBeta, 1, request.stateSize, "attn_ln.bias");
    CHECK(selfQueryWeight, request.stateSize, request.stateSize, "attn.query.weight");
    CHECK(selfQueryBias, 1, request.stateSize, "attn.query.bias");
    CHECK(selfKeyWeight, request.stateSize, request.stateSize, "attn.key.weight");
    CHECK(selfValueWeight, request.stateSize, request.stateSize, "attn.value.weight");
    CHECK(selfValueBias, 1, request.stateSize, "attn.value.bias");
    CHECK(selfAttnOutWeight, request.stateSize, request.stateSize, "attn.out.weight");
    CHECK(selfAttnOutBias, 1, request.stateSize, "attn.out.bias");
    // Cross-attention.
    CHECK(crossAttnLnGamma, 1, request.stateSize, "cross_attn_ln.weight");
    CHECK(crossAttnLnBeta, 1, request.stateSize, "cross_attn_ln.bias");
    CHECK(crossQueryWeight, request.stateSize, request.stateSize, "cross_attn.query.weight");
    CHECK(crossQueryBias, 1, request.stateSize, "cross_attn.query.bias");
    CHECK(crossKeyWeight, request.stateSize, request.stateSize, "cross_attn.key.weight");
    CHECK(crossValueWeight, request.stateSize, request.stateSize, "cross_attn.value.weight");
    CHECK(crossValueBias, 1, request.stateSize, "cross_attn.value.bias");
    CHECK(crossAttnOutWeight, request.stateSize, request.stateSize, "cross_attn.out.weight");
    CHECK(crossAttnOutBias, 1, request.stateSize, "cross_attn.out.bias");
    // FFN.
    CHECK(ffnLnGamma, 1, request.stateSize, "mlp_ln.weight");
    CHECK(ffnLnBeta, 1, request.stateSize, "mlp_ln.bias");
    CHECK(ffn1Weight, ffnSize, request.stateSize, "mlp.0.weight");
    CHECK(ffn1Bias, 1, ffnSize, "mlp.0.bias");
    CHECK(ffn2Weight, request.stateSize, ffnSize, "mlp.2.weight");
    CHECK(ffn2Bias, 1, request.stateSize, "mlp.2.bias");

#undef CHECK
    return true;
}

bool validateWeightDimensions(const CpuWhisperModelWeights &weights, QString *mismatch)
{
    const CpuModelConfig &c = weights.config;

    if (!expectShape(weights.encoder.conv1Weight, c.audioStateSize, c.melBands * 3,
                     "encoder.conv1.weight", mismatch)
        || !expectShape(weights.encoder.conv1Bias, 1, c.audioStateSize,
                        "encoder.conv1.bias", mismatch)
        || !expectShape(weights.encoder.conv2Weight, c.audioStateSize, c.audioStateSize * 3,
                        "encoder.conv2.weight", mismatch)
        || !expectShape(weights.encoder.conv2Bias, 1, c.audioStateSize,
                        "encoder.conv2.bias", mismatch)
        || !expectShape(weights.encoder.positionalEmbedding, c.audioContextSize, c.audioStateSize,
                        "encoder.positional_embedding", mismatch)
        || !expectShape(weights.encoder.lnPostGamma, 1, c.audioStateSize,
                        "encoder.ln_post.weight", mismatch)
        || !expectShape(weights.encoder.lnPostBeta, 1, c.audioStateSize,
                        "encoder.ln_post.bias", mismatch)) {
        return false;
    }

    for (int i = 0; i < c.audioLayerCount; ++i) {
        if (!validateEncoderLayerDimensions(weights.encoder.layers.at(static_cast<std::size_t>(i)),
                                            LayerDimensionValidationRequest{
                                                .stateSize = c.audioStateSize,
                                                .layerIndex = i,
                                                .mismatch = mismatch,
                                            })) {
            return false;
        }
    }

    if (!expectShape(weights.decoder.tokenEmbedding, c.vocabularySize, c.textStateSize,
                     "decoder.token_embedding.weight", mismatch)
        || !expectShape(weights.decoder.positionalEmbedding, c.textContextSize, c.textStateSize,
                        "decoder.positional_embedding", mismatch)
        || !expectShape(weights.decoder.lnGamma, 1, c.textStateSize,
                        "decoder.ln.weight", mismatch)
        || !expectShape(weights.decoder.lnBeta, 1, c.textStateSize,
                        "decoder.ln.bias", mismatch)) {
        return false;
    }

    for (int i = 0; i < c.textLayerCount; ++i) {
        if (!validateDecoderLayerDimensions(weights.decoder.layers.at(static_cast<std::size_t>(i)),
                                            LayerDimensionValidationRequest{
                                                .stateSize = c.textStateSize,
                                                .layerIndex = i,
                                                .mismatch = mismatch,
                                            })) {
            return false;
        }
    }

    return true;
}

} // namespace

std::shared_ptr<CpuWhisperModelWeights>
loadCpuWhisperModelWeights(const QString &path, RuntimeError *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Failed to open native CPU tensor weights"),
                                      QFileInfo(path).absoluteFilePath());
        }
        return nullptr;
    }

    // Read header.
    CpuTensorFileHeader header;
    if (!readExact(&file, &header, sizeof(header))) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Native CPU tensor file header is truncated"),
                                      path);
        }
        return nullptr;
    }

    if (header.magic != kMkcpur3Magic || header.version != kMkcpur3Version) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Native CPU tensor file has invalid magic or version"),
                                      path);
        }
        return nullptr;
    }

    // Read JSON metadata.
    QByteArray metadataBytes(static_cast<qsizetype>(header.metadataBytes), Qt::Uninitialized);
    if (!readExact(&file, metadataBytes.data(), header.metadataBytes)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Native CPU tensor metadata is truncated"),
                                      path);
        }
        return nullptr;
    }

    const QJsonDocument metadataDoc = QJsonDocument::fromJson(metadataBytes);
    if (!metadataDoc.isObject()) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Native CPU tensor metadata is not valid JSON"),
                                      path);
        }
        return nullptr;
    }

    const CpuModelConfig config = parseModelConfig(metadataDoc.object());

    // Read tensor directory.
    std::vector<CpuTensorDirectoryEntry> directory;
    if (!readTensorDirectory(&file, &directory, header.tensorCount)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Native CPU tensor directory is malformed"),
                                      path);
        }
        return nullptr;
    }

    const std::int64_t dataSectionOffset = file.pos();

    auto weights = std::make_shared<CpuWhisperModelWeights>();
    weights->config = config;

    // Load mel filters.
    if (!loadMelFilters(&file, directory, dataSectionOffset, &weights->melFilters, config)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Failed to load mel filterbank from model"),
                                      path);
        }
        return nullptr;
    }

    auto load = [&](QStringView name, CpuTensor *t) {
        return loadNamedTensor(&file, directory, dataSectionOffset, name, t);
    };

    // Encoder.
    if (!load(u"encoder.conv1.weight", &weights->encoder.conv1Weight)
        || !load(u"encoder.conv1.bias", &weights->encoder.conv1Bias)
        || !load(u"encoder.conv2.weight", &weights->encoder.conv2Weight)
        || !load(u"encoder.conv2.bias", &weights->encoder.conv2Bias)
        || !load(u"encoder.positional_embedding", &weights->encoder.positionalEmbedding)
        || !load(u"encoder.ln_post.weight", &weights->encoder.lnPostGamma)
        || !load(u"encoder.ln_post.bias", &weights->encoder.lnPostBeta)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Failed to load encoder weights"),
                                      path);
        }
        return nullptr;
    }

    weights->encoder.layers.resize(static_cast<std::size_t>(config.audioLayerCount));
    for (int i = 0; i < config.audioLayerCount; ++i) {
        if (!loadEncoderLayerWeights(CpuLayerLoadRequest{
                                         .file = &file,
                                         .directory = directory,
                                         .dataSectionOffset = dataSectionOffset,
                                         .layerIndex = i,
                                     },
                                     &weights->encoder.layers.at(static_cast<std::size_t>(i)))) {
            if (error != nullptr) {
                *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                          QStringLiteral("Failed to load encoder layer %1 weights").arg(i),
                                          path);
            }
            return nullptr;
        }
    }

    // Decoder.
    if (!load(u"decoder.token_embedding.weight", &weights->decoder.tokenEmbedding)
        || !load(u"decoder.positional_embedding", &weights->decoder.positionalEmbedding)
        || !load(u"decoder.ln.weight", &weights->decoder.lnGamma)
        || !load(u"decoder.ln.bias", &weights->decoder.lnBeta)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Failed to load decoder weights"),
                                      path);
        }
        return nullptr;
    }

    weights->decoder.layers.resize(static_cast<std::size_t>(config.textLayerCount));
    for (int i = 0; i < config.textLayerCount; ++i) {
        if (!loadDecoderLayerWeights(CpuLayerLoadRequest{
                                         .file = &file,
                                         .directory = directory,
                                         .dataSectionOffset = dataSectionOffset,
                                         .layerIndex = i,
                                     },
                                     &weights->decoder.layers.at(static_cast<std::size_t>(i)))) {
            if (error != nullptr) {
                *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                          QStringLiteral("Failed to load decoder layer %1 weights").arg(i),
                                          path);
            }
            return nullptr;
        }
    }

    // Validate all tensor dimensions against the model config.
    QString dimensionMismatch;
    if (!validateWeightDimensions(*weights, &dimensionMismatch)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Weight dimension mismatch"),
                                      dimensionMismatch);
        }
        return nullptr;
    }

    return weights;
}
