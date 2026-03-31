#include "transcription/runtimeselector.h"

#include "transcription/modelcatalog.h"

#include <utility>

namespace {

RuntimeSelection makeCpuSelection(QString reason, std::optional<ValidatedModelPackage> package = std::nullopt)
{
    return RuntimeSelection{
        .kind = RuntimeSelectionKind::CpuReference,
        .reason = std::move(reason),
        .inspectedPackage = std::move(package),
    };
}

RuntimeSelection makeLegacySelection(QString reason, std::optional<ValidatedModelPackage> package = std::nullopt)
{
    return RuntimeSelection{
        .kind = RuntimeSelectionKind::LegacyWhisper,
        .reason = std::move(reason),
        .inspectedPackage = std::move(package),
    };
}

bool supportsNativeCpuRuntime(const ModelPackageManifest &manifest)
{
    return modelPackageSupportsCompatibility(manifest, cpuReferenceEngineName(), cpuReferenceModelFormat());
}

bool supportsNativeCpuFixtureRuntime(const ModelPackageManifest &manifest)
{
    return modelPackageSupportsCompatibility(manifest, cpuReferenceEngineName(), cpuReferenceFixtureModelFormat());
}

} // namespace

RuntimeSelection selectRuntimeForConfig(const TranscriberConfig &config)
{
    const std::optional<ValidatedModelPackage> package = ModelCatalog::inspectPath(config.modelPath);
    if (!package.has_value()) {
#if defined(MUTTERKEY_WITH_LEGACY_WHISPER)
        return makeLegacySelection(QStringLiteral("Falling back to legacy whisper runtime because the model path could not be inspected"));
#else
        return makeCpuSelection(QStringLiteral("Using native CPU reference runtime because legacy whisper support is disabled"));
#endif
    }

    if (supportsNativeCpuRuntime(package->manifest)) {
        return makeCpuSelection(QStringLiteral("Selected native CPU decoder runtime from package compatibility markers"), package);
    }

    if (supportsNativeCpuFixtureRuntime(package->manifest)) {
        return makeCpuSelection(QStringLiteral("Selected native CPU fixture runtime from legacy native compatibility markers"), package);
    }

    if (modelPackageSupportsCompatibility(package->manifest, legacyWhisperEngineName(), legacyWhisperModelFormat())) {
#if defined(MUTTERKEY_WITH_LEGACY_WHISPER)
        return makeLegacySelection(QStringLiteral("Selected legacy whisper runtime from package compatibility markers"), package);
#else
        return makeCpuSelection(QStringLiteral("Using native CPU reference runtime because legacy whisper support is disabled"), package);
#endif
    }

#if defined(MUTTERKEY_WITH_LEGACY_WHISPER)
    return makeLegacySelection(QStringLiteral("Falling back to legacy whisper runtime because the package markers are not yet recognized by the native selector"),
                               package);
#else
    return makeCpuSelection(QStringLiteral("Using native CPU reference runtime because no compatible runtime markers were recognized"),
                            package);
#endif
}
