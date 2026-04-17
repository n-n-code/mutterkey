#include "asr/model/modelpackage.h"
#include "asr/model/modelvalidator.h"
#include "asr/nativecpu/cpudecoderruntime.h"
#include "asr/nativecpu/cpureferencemodel.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <sys/resource.h>
#include <vector>

namespace {

struct BenchArgs {
    QString packagePath;
    QString audioPath;
    int warmupRuns = 1;
    int measuredRuns = 3;
    int maxDecoderTokens = 96;
    int maxMelFrames = 1200;
};

struct DecodeTiming {
    qint64 elapsedMs = 0;
    QString transcript;
};

void printUsage(QTextStream &stream)
{
    stream << "Usage: nativecpubench --package <dir> --audio <wav> [--warmup N] [--runs M] [--max-tokens N] [--max-mel-frames N]\n";
}

std::optional<QString> optionValue(const QStringList &arguments, const QString &name)
{
    const qsizetype index = arguments.indexOf(name);
    if (index < 0 || index + 1 >= arguments.size()) {
        return std::nullopt;
    }
    return arguments.at(index + 1);
}

std::optional<int> positiveIntOption(const QStringList &arguments, const QString &name, int defaultValue, QString *error)
{
    const std::optional<QString> value = optionValue(arguments, name);
    if (!value.has_value()) {
        return defaultValue;
    }

    bool ok = false;
    const int parsed = value->toInt(&ok);
    if (!ok || parsed < 0) {
        if (error != nullptr) {
            *error = QStringLiteral("%1 must be a non-negative integer").arg(name);
        }
        return std::nullopt;
    }
    return parsed;
}

std::optional<BenchArgs> parseArgs(const QStringList &arguments, QString *error)
{
    const std::optional<QString> packagePath = optionValue(arguments, QStringLiteral("--package"));
    const std::optional<QString> audioPath = optionValue(arguments, QStringLiteral("--audio"));
    if (!packagePath.has_value() || !audioPath.has_value()) {
        if (error != nullptr) {
            *error = QStringLiteral("--package and --audio are required");
        }
        return std::nullopt;
    }

    const std::optional<int> warmup =
        positiveIntOption(arguments, QStringLiteral("--warmup"), 1, error);
    const std::optional<int> runs =
        positiveIntOption(arguments, QStringLiteral("--runs"), 3, error);
    const std::optional<int> maxTokens =
        positiveIntOption(arguments, QStringLiteral("--max-tokens"), 96, error);
    const std::optional<int> maxMelFrames =
        positiveIntOption(arguments, QStringLiteral("--max-mel-frames"), 1200, error);
    if (!warmup.has_value() || !runs.has_value() || !maxTokens.has_value() || !maxMelFrames.has_value()) {
        return std::nullopt;
    }
    if (*runs == 0) {
        if (error != nullptr) {
            *error = QStringLiteral("--runs must be greater than zero");
        }
        return std::nullopt;
    }

    return BenchArgs{
        .packagePath = *packagePath,
        .audioPath = *audioPath,
        .warmupRuns = *warmup,
        .measuredRuns = *runs,
        .maxDecoderTokens = *maxTokens,
        .maxMelFrames = *maxMelFrames,
    };
}

void setError(QString *out, const QString &message)
{
    if (out != nullptr) {
        *out = message;
    }
}

bool readChunkTag(QDataStream &stream, std::array<char, 4> &tag)
{
    return stream.readRawData(tag.data(), static_cast<int>(tag.size())) == static_cast<int>(tag.size());
}

bool tagEquals(const std::array<char, 4> &tag, const char *expected)
{
    for (std::size_t index = 0; index < tag.size(); ++index) {
        if (tag.at(index) != expected[index]) {
            return false;
        }
    }
    return true;
}

std::optional<std::vector<float>> readMono16kHzPcmWav(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Failed to open WAV: %1").arg(file.errorString()));
        return std::nullopt;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);

    std::array<char, 4> riff{};
    std::uint32_t riffSize = 0;
    std::array<char, 4> wave{};
    if (!readChunkTag(stream, riff)) {
        setError(error, QStringLiteral("Truncated RIFF tag in %1").arg(path));
        return std::nullopt;
    }
    stream >> riffSize;
    if (stream.status() != QDataStream::Ok || !readChunkTag(stream, wave)) {
        setError(error, QStringLiteral("Truncated RIFF header in %1").arg(path));
        return std::nullopt;
    }
    if (!tagEquals(riff, "RIFF") || !tagEquals(wave, "WAVE")) {
        setError(error, QStringLiteral("Not a RIFF/WAVE file: %1").arg(path));
        return std::nullopt;
    }

    bool sawFormat = false;
    while (!stream.atEnd()) {
        std::array<char, 4> chunkId{};
        std::uint32_t chunkSize = 0;
        if (!readChunkTag(stream, chunkId)) {
            break;
        }
        stream >> chunkSize;
        if (stream.status() != QDataStream::Ok) {
            break;
        }

        if (tagEquals(chunkId, "fmt ")) {
            if (chunkSize < 16) {
                setError(error, QStringLiteral("fmt chunk too small in %1").arg(path));
                return std::nullopt;
            }
            std::uint16_t audioFormat = 0;
            std::uint16_t numChannels = 0;
            std::uint32_t sampleRate = 0;
            std::uint32_t byteRate = 0;
            std::uint16_t blockAlign = 0;
            std::uint16_t bitsPerSample = 0;
            stream >> audioFormat >> numChannels >> sampleRate >> byteRate >> blockAlign >> bitsPerSample;
            const qint64 fmtExtra = static_cast<qint64>(chunkSize) - 16;
            if (stream.status() != QDataStream::Ok || (fmtExtra > 0 && stream.skipRawData(static_cast<int>(fmtExtra)) != fmtExtra)) {
                setError(error, QStringLiteral("Truncated fmt chunk in %1").arg(path));
                return std::nullopt;
            }
            if (audioFormat != 1 || numChannels != 1 || sampleRate != 16000 || bitsPerSample != 16) {
                setError(error, QStringLiteral("Expected 16 kHz mono 16-bit PCM WAV: %1").arg(path));
                return std::nullopt;
            }
            sawFormat = true;
        } else if (tagEquals(chunkId, "data")) {
            if (!sawFormat) {
                setError(error, QStringLiteral("data chunk precedes fmt in %1").arg(path));
                return std::nullopt;
            }
            const qint64 sampleCount = static_cast<qint64>(chunkSize) / 2;
            std::vector<float> samples(static_cast<std::size_t>(sampleCount));
            constexpr float kInt16Scale = 1.0F / 32768.0F;
            for (qint64 index = 0; index < sampleCount; ++index) {
                std::int16_t sample = 0;
                stream >> sample;
                if (stream.status() != QDataStream::Ok) {
                    setError(error, QStringLiteral("Truncated data chunk in %1").arg(path));
                    return std::nullopt;
                }
                samples.at(static_cast<std::size_t>(index)) = static_cast<float>(sample) * kInt16Scale;
            }
            if ((chunkSize % 2) != 0) {
                stream.skipRawData(1);
            }
            return samples;
        } else {
            const qint64 advance = static_cast<qint64>(chunkSize) + ((chunkSize % 2) != 0 ? 1 : 0);
            if (stream.skipRawData(static_cast<int>(advance)) != advance) {
                break;
            }
        }
    }

    setError(error, QStringLiteral("No data chunk found in %1").arg(path));
    return std::nullopt;
}

QString runtimeErrorText(const RuntimeError &error)
{
    return QStringLiteral("%1: %2").arg(error.message, error.detail);
}

std::optional<std::shared_ptr<const CpuReferenceModelHandle>> loadNativeHandle(const QString &packagePath,
                                                                              qint64 *elapsedMs,
                                                                              QString *error)
{
    QElapsedTimer timer;
    timer.start();

    RuntimeError validationError;
    const std::optional<ValidatedModelPackage> package =
        ModelValidator::validatePackagePath(packagePath, cpuReferenceEngineName(), cpuReferenceModelFormat(), &validationError);
    if (!package.has_value()) {
        setError(error, runtimeErrorText(validationError));
        return std::nullopt;
    }

    RuntimeError loadError;
    const std::shared_ptr<const CpuReferenceModelHandle> handle = loadCpuReferenceModelHandle(*package, &loadError);
    if (handle == nullptr) {
        setError(error, runtimeErrorText(loadError));
        return std::nullopt;
    }
    if (elapsedMs != nullptr) {
        *elapsedMs = timer.elapsed();
    }
    return handle;
}

std::optional<DecodeTiming> decodeOnce(const CpuReferenceModelHandle &handle,
                                       const std::vector<float> &samples,
                                       int maxDecoderTokens,
                                       int maxMelFrames,
                                       QString *error)
{
    if (handle.model().tensorWeights == nullptr) {
        setError(error, QStringLiteral("Loaded package does not contain real decoder weights"));
        return std::nullopt;
    }

    CpuKVCache cache;
    cache.allocate(handle.model().tensorWeights->config, handle.model().tensorWeights->config.textContextSize);

    QElapsedTimer timer;
    timer.start();
    const std::optional<CpuDecodeResult> result = runCpuDecodePass(CpuDecodeRequest{
        .samples = samples,
        .model = &handle.model(),
        .execution = &handle.execution(),
        .kvCache = &cache,
        .sampleRate = 16000,
        .maxDecoderTokens = maxDecoderTokens,
        .maxMelFrames = maxMelFrames,
    });
    const qint64 elapsedMs = timer.elapsed();

    if (!result.has_value()) {
        setError(error, QStringLiteral("Native decoder returned no transcript"));
        return std::nullopt;
    }
    return DecodeTiming{
        .elapsedMs = elapsedMs,
        .transcript = result->transcript,
    };
}

qint64 peakRssKiB()
{
    auto resourceUsageFallback = []() -> qint64 {
        struct rusage usage {};
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
            return usage.ru_maxrss;
        }
        return -1;
    };

    QFile statusFile(QStringLiteral("/proc/self/status"));
    if (!statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return resourceUsageFallback();
    }

    while (!statusFile.atEnd()) {
        const QString line = QString::fromUtf8(statusFile.readLine()).simplified();
        if (!line.startsWith(QStringLiteral("VmHWM:"))) {
            continue;
        }
        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            bool ok = false;
            const qint64 value = parts.at(1).toLongLong(&ok);
            return ok ? value : -1;
        }
    }
    return resourceUsageFallback();
}

qint64 percentile(std::vector<qint64> values, double fraction)
{
    if (values.empty()) {
        return 0;
    }
    std::ranges::sort(values);
    const auto index = static_cast<std::size_t>(
        std::clamp<double>(std::ceil(fraction * static_cast<double>(values.size())) - 1.0, 0.0, static_cast<double>(values.size() - 1)));
    return values.at(index);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    QString parseError;
    const std::optional<BenchArgs> args = parseArgs(app.arguments(), &parseError);
    if (!args.has_value()) {
        err << parseError << "\n";
        printUsage(err);
        return 2;
    }

    QString wavError;
    const std::optional<std::vector<float>> samples = readMono16kHzPcmWav(args->audioPath, &wavError);
    if (!samples.has_value()) {
        err << wavError << "\n";
        return 1;
    }

    qint64 coldLoadMs = 0;
    QString loadError;
    const std::optional<std::shared_ptr<const CpuReferenceModelHandle>> handle =
        loadNativeHandle(args->packagePath, &coldLoadMs, &loadError);
    if (!handle.has_value()) {
        err << loadError << "\n";
        return 1;
    }

    QString decodeError;
    QString transcript;
    for (int run = 0; run < args->warmupRuns; ++run) {
        const std::optional<DecodeTiming> warmup =
            decodeOnce(**handle, *samples, args->maxDecoderTokens, args->maxMelFrames, &decodeError);
        if (!warmup.has_value()) {
            err << decodeError << "\n";
            return 1;
        }
        transcript = warmup->transcript;
    }

    std::vector<qint64> runMs;
    runMs.reserve(static_cast<std::size_t>(args->measuredRuns));
    QJsonArray runsJson;
    for (int run = 0; run < args->measuredRuns; ++run) {
        const std::optional<DecodeTiming> timing =
            decodeOnce(**handle, *samples, args->maxDecoderTokens, args->maxMelFrames, &decodeError);
        if (!timing.has_value()) {
            err << decodeError << "\n";
            return 1;
        }
        transcript = timing->transcript;
        runMs.push_back(timing->elapsedMs);
        runsJson.append(timing->elapsedMs);
    }

    const qint64 minMs = *std::ranges::min_element(runMs);
    const qint64 medianMs = percentile(runMs, 0.50);
    const qint64 p95Ms = percentile(runMs, 0.95);
    const qint64 rssKiB = peakRssKiB();

    QJsonObject resultJson{
        {QStringLiteral("package"), QFileInfo(args->packagePath).absoluteFilePath()},
        {QStringLiteral("audio"), QFileInfo(args->audioPath).absoluteFilePath()},
        {QStringLiteral("warmup_runs"), args->warmupRuns},
        {QStringLiteral("measured_runs"), args->measuredRuns},
        {QStringLiteral("max_decoder_tokens"), args->maxDecoderTokens},
        {QStringLiteral("max_mel_frames"), args->maxMelFrames},
        {QStringLiteral("cold_load_ms"), coldLoadMs},
        {QStringLiteral("warm_decode_ms_min"), minMs},
        {QStringLiteral("warm_decode_ms_median"), medianMs},
        {QStringLiteral("warm_decode_ms_p95"), p95Ms},
        {QStringLiteral("peak_rss_kib"), rssKiB},
        {QStringLiteral("transcript"), transcript},
        {QStringLiteral("runs_ms"), runsJson},
    };

    out << "# Native CPU Benchmark\n\n";
    out << "| Metric | Value |\n";
    out << "| --- | ---: |\n";
    out << "| Cold package/model load | " << coldLoadMs << " ms |\n";
    out << "| Warm decode min | " << minMs << " ms |\n";
    out << "| Warm decode median | " << medianMs << " ms |\n";
    out << "| Warm decode p95 | " << p95Ms << " ms |\n";
    out << "| Peak RSS | " << rssKiB << " KiB |\n";
    out << "| Max decoder tokens | " << args->maxDecoderTokens << " |\n";
    out << "| Max mel frames | " << args->maxMelFrames << " |\n\n";
    out << "Transcript:\n\n";
    out << "> " << transcript << "\n\n";
    out << "```json\n";
    out << QString::fromUtf8(QJsonDocument(resultJson).toJson(QJsonDocument::Indented));
    out << "```\n";
    return 0;
}
