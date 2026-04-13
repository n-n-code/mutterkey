#include "app/applicationcommands.h"

#include "config.h"
#include "audio/audiorecorder.h"
#include "audio/recording.h"
#include "clipboardwriter.h"
#include "control/daemoncontrolserver.h"
#include "asr/model/modelcatalog.h"
#include "asr/model/modelpackage.h"
#include "asr/model/rawwhisperimporter.h"
#include "service.h"
#include "asr/streaming/transcriptioncompat.h"
#include "asr/runtime/transcriptionengine.h"
#include "asr/runtime/transcriptiontypes.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QObject>
#include <QString>
#include <QTextStream>
#include <QTimer>
#include <QtLogging>
#include <QtCore/qnamespace.h>

Q_LOGGING_CATEGORY(appLog, "mutterkey.app")

void configureLogging(const QString &level)
{
    qSetMessagePattern(QStringLiteral("%{time yyyy-MM-dd hh:mm:ss.zzz} %{if-debug}DEBUG%{endif}%{if-info}INFO%{endif}%{if-warning}WARNING%{endif}%{if-critical}ERROR%{endif}%{if-fatal}FATAL%{endif} %{category}: %{message}"));

    if (level.compare(QStringLiteral("DEBUG"), Qt::CaseInsensitive) == 0) {
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true"));
    } else {
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
    }
}

int runDaemon(QGuiApplication &app, const AppConfig &config, const QString &configPath)
{
    const std::shared_ptr<const TranscriptionEngine> transcriptionEngine = createTranscriptionEngine(config.transcriber);
    MutterkeyService service(config, transcriptionEngine, QGuiApplication::clipboard());
    DaemonControlServer controlServer(configPath, config, &service);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &service, &MutterkeyService::stop);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &controlServer, &DaemonControlServer::stop);

    QString errorMessage;
    if (!service.start(&errorMessage)) {
        qCCritical(appLog) << "Failed to start daemon:" << errorMessage;
        return 1;
    }
    if (!controlServer.start(&errorMessage)) {
        qCCritical(appLog) << "Failed to start daemon control server:" << errorMessage;
        service.stop();
        return 1;
    }

    qCInfo(appLog) << "Mutterkey daemon running. Hold" << config.shortcut.sequence << "to talk.";
    return QGuiApplication::exec();
}

int runOnce(QGuiApplication &app, const AppConfig &config, double seconds)
{
    AudioRecorder recorder(config.audio);
    const std::shared_ptr<const TranscriptionEngine> transcriptionEngine = createTranscriptionEngine(config.transcriber);
    RuntimeError runtimeError;
    const std::shared_ptr<const TranscriptionModelHandle> model = transcriptionEngine->loadModel(&runtimeError);
    if (model == nullptr) {
        qCCritical(appLog) << "Failed to load transcription model:" << runtimeError.message;
        return 1;
    }

    std::unique_ptr<TranscriptionSession> transcriber = transcriptionEngine->createSession(model);
    if (transcriber == nullptr) {
        qCCritical(appLog) << "Failed to create transcription session";
        return 1;
    }
    ClipboardWriter clipboardWriter(QGuiApplication::clipboard());

    if (config.transcriber.warmupOnStart) {
        if (!transcriber->warmup(&runtimeError)) {
            qCCritical(appLog) << "Failed to warm up transcriber:" << runtimeError.message;
            return 1;
        }
    }

    QTimer::singleShot(0,
                       &app,
                       [&app, &recorder, transcriber = transcriber.get(), &clipboardWriter, seconds, normalizer = RecordingNormalizer()]() {
        QString errorMessage;
        if (!recorder.start(&errorMessage)) {
            qCCritical(appLog) << "Failed to start one-shot recording:" << errorMessage;
            QGuiApplication::exit(1);
            return;
        }

        qCInfo(appLog) << "Recording for" << seconds << "seconds";
        QTimer::singleShot(static_cast<int>(seconds * 1000), &app, [&app, &recorder, transcriber, &clipboardWriter, normalizer]() {
            const Recording recording = recorder.stop();
            if (!recording.isValid()) {
                qCCritical(appLog) << "Recorder returned no audio";
                QGuiApplication::exit(1);
                return;
            }

            const TranscriptionResult result = transcribeRecordingViaStreaming(*transcriber, recording, normalizer);
            if (!result.success) {
                qCCritical(appLog) << "One-shot transcription failed:" << result.error.message;
                QGuiApplication::exit(1);
                return;
            }

            if (!result.text.trimmed().isEmpty()) {
                const QString trimmedText = result.text.trimmed();
                if (!clipboardWriter.copy(trimmedText)) {
                    qCWarning(appLog) << "Clipboard update appears to have failed";
                }
                QTextStream(stdout) << trimmedText << Qt::endl;
            } else {
                qCInfo(appLog) << "No speech detected";
            }
            QGuiApplication::exit(0);
        });
    });

    return QGuiApplication::exec();
}

int runDiagnose(QGuiApplication &app, const AppConfig &config, double seconds, bool invokeShortcut)
{
    const std::shared_ptr<const TranscriptionEngine> transcriptionEngine = createTranscriptionEngine(config.transcriber);
    MutterkeyService service(config, transcriptionEngine, QGuiApplication::clipboard());
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &service, &MutterkeyService::stop);

    QString errorMessage;
    if (!service.start(&errorMessage)) {
        qCCritical(appLog) << "Diagnostic startup failed:" << errorMessage;
        return 1;
    }

    qCInfo(appLog) << "Diagnostic mode active for" << seconds << "seconds. Press the configured shortcut now.";
    if (invokeShortcut) {
        QTimer::singleShot(750, &app, [&service]() {
            QString invokeError;
            if (!service.invokeShortcut(&invokeError)) {
                qCWarning(appLog) << "Diagnostic shortcut invoke failed:" << invokeError;
            } else {
                qCInfo(appLog) << "Invoked the registered shortcut through KGlobalAccel";
            }
        });
    }

    QTimer::singleShot(static_cast<int>(seconds * 1000), &app, [&app, &service]() {
        QTextStream(stdout) << QJsonDocument(service.diagnostics()).toJson(QJsonDocument::Indented);
        QGuiApplication::exit(0);
    });

    return QGuiApplication::exec();
}

namespace {

QJsonObject metadataToJson(const ModelMetadata &metadata)
{
    return QJsonObject{
        {QStringLiteral("package_id"), metadata.packageId},
        {QStringLiteral("display_name"), metadata.displayName},
        {QStringLiteral("package_version"), metadata.packageVersion},
        {QStringLiteral("runtime_family"), metadata.runtimeFamily},
        {QStringLiteral("source_format"), metadata.sourceFormat},
        {QStringLiteral("model_format"), metadata.modelFormat},
        {QStringLiteral("architecture"), metadata.architecture},
        {QStringLiteral("language_profile"), metadata.languageProfile},
        {QStringLiteral("quantization"), metadata.quantization},
        {QStringLiteral("tokenizer"), metadata.tokenizer},
        {QStringLiteral("legacy_compatibility"), metadata.legacyCompatibility},
        {QStringLiteral("vocabulary_size"), metadata.vocabularySize},
        {QStringLiteral("audio_context"), metadata.audioContext},
        {QStringLiteral("audio_state"), metadata.audioState},
        {QStringLiteral("audio_head_count"), metadata.audioHeadCount},
        {QStringLiteral("audio_layer_count"), metadata.audioLayerCount},
        {QStringLiteral("text_context"), metadata.textContext},
        {QStringLiteral("text_state"), metadata.textState},
        {QStringLiteral("text_head_count"), metadata.textHeadCount},
        {QStringLiteral("text_layer_count"), metadata.textLayerCount},
        {QStringLiteral("mel_count"), metadata.melCount},
        {QStringLiteral("format_type"), metadata.formatType},
    };
}

QJsonArray compatibilityToJson(const std::vector<ModelCompatibilityMarker> &markers)
{
    QJsonArray array;
    for (const ModelCompatibilityMarker &marker : markers) {
        array.append(QJsonObject{
            {QStringLiteral("engine"), marker.engine},
            {QStringLiteral("model_format"), marker.modelFormat},
        });
    }
    return array;
}

QJsonObject nativeExecutionToJson(const NativeExecutionMetadata &metadata)
{
    return QJsonObject{
        {QStringLiteral("execution_version"), metadata.executionVersion},
        {QStringLiteral("baseline_family"), metadata.baselineFamily},
        {QStringLiteral("decoder"), metadata.decoder},
        {QStringLiteral("tokenizer"), metadata.tokenizer},
        {QStringLiteral("tokenizer_asset_role"), metadata.tokenizerAssetRole},
        {QStringLiteral("tokenizer_merges_asset_role"), metadata.tokenizerMergesAssetRole},
        {QStringLiteral("frontend"), metadata.frontend},
        {QStringLiteral("search_policy"), metadata.searchPolicy},
        {QStringLiteral("timestamp_mode"), metadata.timestampMode},
        {QStringLiteral("feature_bin_count"), metadata.featureBinCount},
        {QStringLiteral("template_count"), metadata.templateCount},
        {QStringLiteral("max_distance"), metadata.maxDistance},
        {QStringLiteral("bos_token_id"), metadata.bosTokenId},
        {QStringLiteral("eos_token_id"), metadata.eosTokenId},
        {QStringLiteral("no_speech_token_id"), metadata.noSpeechTokenId},
        {QStringLiteral("timestamp_token_start_id"), metadata.timestampTokenStartId},
        {QStringLiteral("timestamp_token_end_id"), metadata.timestampTokenEndId},
    };
}

QJsonArray assetsToJson(const std::vector<ModelAssetMetadata> &assets)
{
    QJsonArray array;
    for (const ModelAssetMetadata &asset : assets) {
        array.append(QJsonObject{
            {QStringLiteral("role"), asset.role},
            {QStringLiteral("path"), asset.relativePath},
            {QStringLiteral("sha256"), asset.sha256},
            {QStringLiteral("size_bytes"), asset.sizeBytes},
        });
    }
    return array;
}

} // namespace

int runModelImport(const QString &sourcePath, const QString &outputPath, const QString &packageIdOverride)
{
    RuntimeError runtimeError;
    const std::optional<ValidatedModelPackage> package =
        RawWhisperImporter::importFile(sourcePath,
                                       RawWhisperImportRequest{
                                           .outputPath = outputPath,
                                           .packageIdOverride = packageIdOverride,
                                       },
                                       &runtimeError);
    if (!package.has_value()) {
        qCCritical(appLog) << "Model import failed:" << runtimeError.message;
        return 1;
    }

    QTextStream(stdout) << package->packageRootPath << Qt::endl;
    return 0;
}

int runModelInspect(const QString &path)
{
    RuntimeError runtimeError;
    const std::optional<ValidatedModelPackage> package = ModelCatalog::inspectPath(path, {}, {}, &runtimeError);
    if (!package.has_value()) {
        qCCritical(appLog) << "Model inspect failed:" << runtimeError.message;
        return 1;
    }

    QJsonObject object;
    object.insert(QStringLiteral("package_root"), package->packageRootPath);
    object.insert(QStringLiteral("manifest_path"), package->manifestPath);
    object.insert(QStringLiteral("source_path"), package->sourcePath);
    object.insert(QStringLiteral("weights_path"), package->weightsPath);
    object.insert(QStringLiteral("description"), package->description());
    object.insert(QStringLiteral("metadata"), metadataToJson(package->metadata()));
    object.insert(QStringLiteral("native_execution"), nativeExecutionToJson(package->nativeExecution()));
    object.insert(QStringLiteral("compatible_engines"), compatibilityToJson(package->manifest.compatibleEngines));
    object.insert(QStringLiteral("assets"), assetsToJson(package->manifest.assets));
    QTextStream(stdout) << QJsonDocument(object).toJson(QJsonDocument::Indented);
    return 0;
}
