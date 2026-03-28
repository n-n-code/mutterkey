#include "transcription/cpureferencetranscriber.h"
#include "transcription/modelpackage.h"
#include "transcription/transcriptionengine.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <array>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::array<char, 8> kCpuReferenceMagic{'M', 'K', 'C', 'P', 'U', 'R', '1', '\0'};
constexpr std::uint32_t kCpuReferenceVersion = 1;

class CpuReferenceTranscriberTest final : public QObject
{
    Q_OBJECT

private slots:
    void engineDispatchesToNativeCpuPackage();
    void nativeCpuRuntimeEmitsDeterministicTranscript();
    void nativeCpuRuntimeRejectsTranslationMode();
    void engineDiagnosticsExplainNativeSelection();
};

template <typename T>
bool writePod(QFile *file, const T &value)
{
    if (file == nullptr) {
        return false;
    }

    QByteArray bytes(static_cast<qsizetype>(sizeof(T)), '\0');
    std::memcpy(bytes.data(), &value, sizeof(T));
    return file->write(bytes) == bytes.size();
}

struct CpuReferenceFixtureFile {
    QString path;
    QString transcript;
};

struct CpuReferenceFixturePackage {
    QString packageRoot;
    QString transcript;
};

QString sha256ForFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}

bool writeCpuReferenceModel(const CpuReferenceFixtureFile &request)
{
    QFile file(request.path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    if (file.write(kCpuReferenceMagic.data(), static_cast<qint64>(kCpuReferenceMagic.size()))
        != static_cast<qint64>(kCpuReferenceMagic.size())) {
        return false;
    }

    const QByteArray transcriptUtf8 = request.transcript.toUtf8();
    const auto transcriptBytes = static_cast<std::uint32_t>(transcriptUtf8.size());
    return writePod(&file, kCpuReferenceVersion)
        && writePod(&file, transcriptBytes)
        && file.write(transcriptUtf8) == transcriptUtf8.size();
}

bool writeCpuReferencePackage(const CpuReferenceFixturePackage &request)
{
    const QDir root;
    if (!root.mkpath(QDir(request.packageRoot).filePath(QStringLiteral("assets")))) {
        return false;
    }

    const QString weightsPath = QDir(request.packageRoot).filePath(QStringLiteral("assets/model.mkcpu"));
    if (!writeCpuReferenceModel(CpuReferenceFixtureFile{
            .path = weightsPath,
            .transcript = request.transcript,
        })) {
        return false;
    }

    ModelPackageManifest manifest;
    manifest.format = QStringLiteral("mutterkey.model-package");
    manifest.schemaVersion = 1;
    manifest.metadata.packageId = QStringLiteral("fixture-native-cpu");
    manifest.metadata.displayName = QStringLiteral("Fixture Native CPU");
    manifest.metadata.runtimeFamily = QStringLiteral("asr");
    manifest.metadata.sourceFormat = QStringLiteral("mutterkey-native");
    manifest.metadata.modelFormat = cpuReferenceModelFormat();
    manifest.metadata.architecture = QStringLiteral("reference-fixture");
    manifest.metadata.languageProfile = QStringLiteral("en");
    manifest.metadata.tokenizer = QStringLiteral("builtin");
    manifest.compatibleEngines.push_back(ModelCompatibilityMarker{
        .engine = cpuReferenceEngineName(),
        .modelFormat = cpuReferenceModelFormat(),
    });
    manifest.assets.push_back(ModelAssetMetadata{
        .role = QStringLiteral("weights"),
        .relativePath = QStringLiteral("assets/model.mkcpu"),
        .sha256 = sha256ForFile(weightsPath),
        .sizeBytes = QFileInfo(weightsPath).size(),
    });

    QFile manifestFile(QDir(request.packageRoot).filePath(QStringLiteral("model.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    manifestFile.write(QJsonDocument(modelPackageManifestToJson(manifest)).toJson(QJsonDocument::Indented));
    return true;
}

AudioChunk validChunk()
{
    return AudioChunk{
        .samples = {0.0F, 0.05F, -0.04F, 0.03F, -0.02F, 0.01F, -0.03F, 0.02F},
        .sampleRate = 16000,
        .channels = 1,
        .streamOffsetFrames = 0,
    };
}

} // namespace

void CpuReferenceTranscriberTest::engineDispatchesToNativeCpuPackage()
{
    // WHAT: Verify that the generic engine factory chooses the native CPU runtime for a native package.
    // HOW: Create a temporary package marked for the product-owned CPU runtime and inspect the backend name exposed by the factory.
    // WHY: Phase 5 only starts decoupling from whisper.cpp once the app-level factory routes native packages to the new runtime contract.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString packageRoot = tempDir.filePath(QStringLiteral("native-package"));
    QVERIFY(writeCpuReferencePackage(CpuReferenceFixturePackage{
        .packageRoot = packageRoot,
        .transcript = QStringLiteral("hello from cpu runtime"),
    }));

    TranscriberConfig config;
    config.modelPath = packageRoot;

    const std::shared_ptr<const TranscriptionEngine> engine = createTranscriptionEngine(config);
    QVERIFY(engine != nullptr);
    QCOMPARE(engine->capabilities().backendName, cpuReferenceEngineName());
}

void CpuReferenceTranscriberTest::nativeCpuRuntimeEmitsDeterministicTranscript()
{
    // WHAT: Verify that the native CPU runtime loads a package and emits a deterministic final transcript.
    // HOW: Build a temporary native package whose weights payload embeds a fixed transcript, then push one valid chunk and finish the session.
    // WHY: The Phase 5 reference runtime needs deterministic model/session behavior so conformance tests can pin transcript and timestamp behavior without third-party state.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString packageRoot = tempDir.filePath(QStringLiteral("native-package"));
    QVERIFY(writeCpuReferencePackage(CpuReferenceFixturePackage{
        .packageRoot = packageRoot,
        .transcript = QStringLiteral("hello from cpu runtime"),
    }));

    TranscriberConfig config;
    config.modelPath = packageRoot;

    RuntimeError error;
    const std::shared_ptr<const TranscriptionModelHandle> model = CpuReferenceTranscriber::loadModelHandle(config, &error);
    QVERIFY(model != nullptr);
    QVERIFY(error.isOk());

    std::unique_ptr<TranscriptionSession> session = CpuReferenceTranscriber::createSession(config, model);
    QVERIFY(session != nullptr);
    QVERIFY(session->warmup(&error));
    QVERIFY(error.isOk());

    const TranscriptUpdate pushUpdate = session->pushAudioChunk(validChunk());
    QVERIFY(pushUpdate.isOk());

    const TranscriptUpdate finishUpdate = session->finish();
    QVERIFY(finishUpdate.isOk());
    QCOMPARE(finishUpdate.events.size(), static_cast<std::size_t>(1));
    QCOMPARE(finishUpdate.events.front().kind, TranscriptEventKind::Final);
    QCOMPARE(finishUpdate.events.front().text, QStringLiteral("hello from cpu runtime"));
    QCOMPARE(finishUpdate.events.front().startMs, 0);
    QVERIFY(finishUpdate.events.front().endMs > 0);
}

void CpuReferenceTranscriberTest::nativeCpuRuntimeRejectsTranslationMode()
{
    // WHAT: Verify that the native CPU runtime rejects translation mode in its narrow Phase 5 scope.
    // HOW: Create a temporary native package, enable translate in config, and finish a session after pushing one valid chunk.
    // WHY: The first product-owned CPU runtime is intentionally ASR-only, so unsupported feature requests must fail explicitly instead of silently diverging from the contract.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString packageRoot = tempDir.filePath(QStringLiteral("native-package"));
    QVERIFY(writeCpuReferencePackage(CpuReferenceFixturePackage{
        .packageRoot = packageRoot,
        .transcript = QStringLiteral("hello from cpu runtime"),
    }));

    TranscriberConfig config;
    config.modelPath = packageRoot;
    config.translate = true;

    RuntimeError error;
    const std::shared_ptr<const TranscriptionModelHandle> model = CpuReferenceTranscriber::loadModelHandle(config, &error);
    QVERIFY(model != nullptr);
    QVERIFY(error.isOk());

    std::unique_ptr<TranscriptionSession> session = CpuReferenceTranscriber::createSession(config, model);
    QVERIFY(session != nullptr);
    QVERIFY(session->pushAudioChunk(validChunk()).isOk());

    const TranscriptUpdate finishUpdate = session->finish();
    QVERIFY(!finishUpdate.isOk());
    QCOMPARE(finishUpdate.error.code, RuntimeErrorCode::InvalidConfig);
}

void CpuReferenceTranscriberTest::engineDiagnosticsExplainNativeSelection()
{
    // WHAT: Verify that native-runtime diagnostics explain why the generic factory chose the native backend.
    // HOW: Create a temporary native package, construct the generic engine, and inspect the selection reason surfaced in runtime diagnostics.
    // WHY: Once runtime policy is moved out of the factory body, diagnostics should preserve that policy decision so diagnose output remains actionable.
    const QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString packageRoot = tempDir.filePath(QStringLiteral("native-package"));
    QVERIFY(writeCpuReferencePackage(CpuReferenceFixturePackage{
        .packageRoot = packageRoot,
        .transcript = QStringLiteral("hello from cpu runtime"),
    }));

    TranscriberConfig config;
    config.modelPath = packageRoot;

    const std::shared_ptr<const TranscriptionEngine> engine = createTranscriptionEngine(config);
    QVERIFY(engine != nullptr);
    QCOMPARE(engine->diagnostics().backendName, cpuReferenceEngineName());
    QVERIFY(engine->diagnostics().selectionReason.contains(QStringLiteral("native CPU reference runtime"),
                                                          Qt::CaseInsensitive));
}

QTEST_APPLESS_MAIN(CpuReferenceTranscriberTest)

#include "cpureferencetranscribertest.moc"
