#include "asr/legacy/whispercpptranscriber.h"
#include "asr/model/modelpackage.h"
#include "asr/model/modelvalidator.h"
#include "asr/nativecpu/cpudecoderruntime.h"
#include "asr/nativecpu/cpureferencemodel.h"
#include "config.h"
#include "testutil/pcmwavreader.h"

#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QtTest/QTest>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <optional>
#include <vector>

namespace {

class CpuWhisperParityTest final : public QObject
{
    Q_OBJECT

private slots:
    void nativeCpuTracksLegacyWhisperOnJfkSample();
};

QString runtimeErrorText(const RuntimeError &error)
{
    return QStringLiteral("%1: %2").arg(error.message, error.detail);
}

QString normalizedTranscript(QString text)
{
    text.replace(QStringLiteral("<|endoftext|>"), QLatin1String(" "));
    text.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9]+")), QStringLiteral(" "));
    return text.toLower().simplified();
}

QStringList transcriptWords(const QString &text)
{
    return normalizedTranscript(text).split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

int tokenLevenshteinDistance(const QStringList &left, const QStringList &right)
{
    std::vector<int> previous(static_cast<std::size_t>(right.size() + 1));
    std::vector<int> current(static_cast<std::size_t>(right.size() + 1));
    for (int index = 0; index <= right.size(); ++index) {
        previous.at(static_cast<std::size_t>(index)) = index;
    }

    for (int row = 1; row <= left.size(); ++row) {
        current.at(0) = row;
        for (int column = 1; column <= right.size(); ++column) {
            const int substitutionCost = left.at(row - 1) == right.at(column - 1) ? 0 : 1;
            current.at(static_cast<std::size_t>(column)) =
                std::min({previous.at(static_cast<std::size_t>(column)) + 1,
                          current.at(static_cast<std::size_t>(column - 1)) + 1,
                          previous.at(static_cast<std::size_t>(column - 1)) + substitutionCost});
        }
        std::swap(previous, current);
    }

    return previous.at(static_cast<std::size_t>(right.size()));
}

double transcriptSimilarity(const QString &left, const QString &right)
{
    const QStringList leftWords = transcriptWords(left);
    const QStringList rightWords = transcriptWords(right);
    if (leftWords.isEmpty() && rightWords.isEmpty()) {
        return 1.0;
    }
    if (leftWords.isEmpty() || rightWords.isEmpty()) {
        return 0.0;
    }

    const int distance = tokenLevenshteinDistance(leftWords, rightWords);
    const qsizetype maxLength = std::max(leftWords.size(), rightWords.size());
    return 1.0 - (static_cast<double>(distance) / static_cast<double>(maxLength));
}

QString updateTranscript(const TranscriptUpdate &update)
{
    QStringList finalTexts;
    QStringList fallbackTexts;
    for (const TranscriptEvent &event : update.events) {
        if (!event.text.trimmed().isEmpty()) {
            fallbackTexts.append(event.text.trimmed());
        }
        if (event.kind == TranscriptEventKind::Final && !event.text.trimmed().isEmpty()) {
            finalTexts.append(event.text.trimmed());
        }
    }
    return (finalTexts.isEmpty() ? fallbackTexts : finalTexts).join(QLatin1Char(' ')).simplified();
}

std::optional<std::vector<float>> loadJfkSamples(QString *error)
{
    const QString wavPath = QString::fromUtf8(MUTTERKEY_TEST_JFK_WAV_PATH);
    if (!QFileInfo::exists(wavPath)) {
        if (error != nullptr) {
            *error = QStringLiteral("JFK WAV fixture not found: %1").arg(wavPath);
        }
        return std::nullopt;
    }
    return readMono16kHzPcmWav(wavPath, error);
}

std::optional<QString> transcribeWithNativePackage(const QString &packagePath,
                                                   const std::vector<float> &samples,
                                                   QString *error)
{
    RuntimeError validationError;
    const std::optional<ValidatedModelPackage> package =
        ModelValidator::validatePackagePath(packagePath, cpuReferenceEngineName(), cpuReferenceModelFormat(), &validationError);
    if (!package.has_value()) {
        if (error != nullptr) {
            *error = runtimeErrorText(validationError);
        }
        return std::nullopt;
    }
    const ValidatedModelPackage &validatedPackage = package.value();

    RuntimeError loadError;
    const std::shared_ptr<const CpuReferenceModelHandle> handle = loadCpuReferenceModelHandle(validatedPackage, &loadError);
    if (handle == nullptr || handle->model().tensorWeights == nullptr) {
        if (error != nullptr) {
            *error = loadError.isOk() ? QStringLiteral("Native package did not load real decoder weights")
                                      : runtimeErrorText(loadError);
        }
        return std::nullopt;
    }

    CpuKVCache cache;
    cache.allocate(handle->model().tensorWeights->config, handle->model().tensorWeights->config.textContextSize);
    const std::optional<CpuDecodeResult> result = runCpuDecodePass(CpuDecodeRequest{
        .samples = samples,
        .model = &handle->model(),
        .execution = &handle->execution(),
        .kvCache = &cache,
        .sampleRate = 16000,
        .maxDecoderTokens = 96,
        .maxMelFrames = 1200,
    });
    if (!result.has_value()) {
        if (error != nullptr) {
            *error = QStringLiteral("Native package returned no transcript");
        }
        return std::nullopt;
    }
    return result->transcript;
}

std::optional<QString> transcribeWithLegacyWhisper(const QString &legacyWeightsPath,
                                                   const std::vector<float> &samples,
                                                   QString *error)
{
    TranscriberConfig config;
    config.modelPath = legacyWeightsPath;
    config.language = QStringLiteral("en");
    config.threads = 1;

    RuntimeError loadError;
    std::shared_ptr<const TranscriptionModelHandle> model = WhisperCppTranscriber::loadModelHandle(config, &loadError);
    if (model == nullptr) {
        if (error != nullptr) {
            *error = runtimeErrorText(loadError);
        }
        return std::nullopt;
    }

    std::unique_ptr<TranscriptionSession> session = WhisperCppTranscriber::createSession(config, std::move(model));
    if (session == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("Failed to create legacy whisper session");
        }
        return std::nullopt;
    }

    const TranscriptUpdate pushUpdate = session->pushAudioChunk(AudioChunk{
        .samples = samples,
        .sampleRate = 16000,
        .channels = 1,
        .streamOffsetFrames = 0,
    });
    if (!pushUpdate.isOk()) {
        if (error != nullptr) {
            *error = runtimeErrorText(pushUpdate.error);
        }
        return std::nullopt;
    }

    const TranscriptUpdate finishUpdate = session->finish();
    if (!finishUpdate.isOk()) {
        if (error != nullptr) {
            *error = runtimeErrorText(finishUpdate.error);
        }
        return std::nullopt;
    }

    QString transcript = updateTranscript(finishUpdate);
    if (transcript.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("Legacy whisper returned no transcript text");
        }
        return std::nullopt;
    }
    return transcript;
}

} // namespace

void CpuWhisperParityTest::nativeCpuTracksLegacyWhisperOnJfkSample()
{
    // WHAT: Compare the native CPU real-decoder transcript with the legacy whisper.cpp transcript on the same JFK fixture.
    // HOW: Decode jfk.wav through the staged native package and local ggml base.en model, then assert clean native text and normalized token similarity.
    // WHY: Phase 5B needs a migration guardrail while whisper.cpp remains the validated user-facing speech decoder.
    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QString packagePath = environment.value(QStringLiteral("MUTTERKEY_TEST_PACKAGE_PATH"));
    const QString legacyWeightsPath = environment.value(QStringLiteral("MUTTERKEY_TEST_LEGACY_WEIGHTS_PATH"));
    if (packagePath.isEmpty()) {
        QSKIP("MUTTERKEY_TEST_PACKAGE_PATH not set — skipping native/legacy parity test");
    }
    if (legacyWeightsPath.isEmpty()) {
        QSKIP("MUTTERKEY_TEST_LEGACY_WEIGHTS_PATH not set — skipping native/legacy parity test");
    }
    if (!QFileInfo::exists(packagePath)) {
        QSKIP(qPrintable(QStringLiteral("Native package path not found: %1").arg(packagePath)));
    }
    if (!QFileInfo::exists(legacyWeightsPath)) {
        QSKIP(qPrintable(QStringLiteral("Legacy weights path not found: %1").arg(legacyWeightsPath)));
    }

    QString fixtureError;
    const std::optional<std::vector<float>> samples = loadJfkSamples(&fixtureError);
    if (!samples.has_value()) {
        QFAIL(qPrintable(fixtureError));
        return;
    }
    const std::vector<float> &loadedSamples = samples.value();

    QString nativeError;
    const std::optional<QString> nativeTranscript = transcribeWithNativePackage(packagePath, loadedSamples, &nativeError);
    if (!nativeTranscript.has_value()) {
        QFAIL(qPrintable(nativeError));
        return;
    }
    const QString &nativeText = nativeTranscript.value();

    QString legacyError;
    const std::optional<QString> legacyTranscript = transcribeWithLegacyWhisper(legacyWeightsPath, loadedSamples, &legacyError);
    if (!legacyTranscript.has_value()) {
        QFAIL(qPrintable(legacyError));
        return;
    }
    const QString &legacyText = legacyTranscript.value();

    constexpr double kMinimumSimilarity = 0.75;
    const double similarity = transcriptSimilarity(nativeText, legacyText);
    qDebug().noquote() << "Native transcript:" << nativeText;
    qDebug().noquote() << "Legacy transcript:" << legacyText;
    qDebug() << "Native/legacy JFK token similarity:" << similarity;

    QVERIFY2(!nativeText.contains(QChar(0x0120)),
             qPrintable(QStringLiteral("Native transcript leaked byte-level space markers: %1").arg(nativeText)));
    QVERIFY2(similarity >= kMinimumSimilarity,
             qPrintable(QStringLiteral("Native/legacy JFK similarity %1 is below threshold %2; native='%3'; legacy='%4'")
                            .arg(similarity, 0, 'f', 3)
                            .arg(kMinimumSimilarity, 0, 'f', 3)
                            .arg(normalizedTranscript(nativeText), normalizedTranscript(legacyText))));
}

QTEST_APPLESS_MAIN(CpuWhisperParityTest)

#include "cpuwhisperparitytest.moc"
