#include "transcription/cpudecoderruntime.h"
#include "transcription/cpufeatureextractor.h"
#include "transcription/cpureferencemodel.h"
#include "transcription/cputimestamps.h"

#include <QtTest/QTest>

namespace {

class CpuDecoderRuntimeTest final : public QObject
{
    Q_OBJECT

private slots:
    void fixtureModelEmitsTokenizedTranscript();
    void templateModelMatchesAudioProfile();
    void timestampTokensDriveFinalEventRange();
};

std::vector<float> makeNormalizedProfile(std::initializer_list<float> values)
{
    const std::vector<float> raw(values);
    return extractCpuEnergyProfile(raw, static_cast<int>(raw.size()));
}

std::vector<float> synthesizeSamples(const std::vector<float> &profile)
{
    constexpr int samplesPerBin = 128;

    std::vector<float> samples;
    samples.reserve(profile.size() * static_cast<std::size_t>(samplesPerBin));
    std::size_t binIndex = 0;
    for (const float amplitude : profile) {
        for (int sampleIndex = 0; sampleIndex < samplesPerBin; ++sampleIndex) {
            const float sign = ((sampleIndex + static_cast<int>(binIndex)) % 2 == 0) ? 1.0F : -1.0F;
            samples.push_back(amplitude * sign);
        }
        ++binIndex;
    }

    return samples;
}

} // namespace

void CpuDecoderRuntimeTest::fixtureModelEmitsTokenizedTranscript()
{
    // WHAT: Verify that the decoder runtime turns the older fixture payload into a final transcript event.
    // HOW: Build an in-memory fixture model and run one decode pass over a non-empty utterance buffer.
    // WHY: The Phase 5B coordinator should keep the legacy native fixture path working while decoder-owned logic moves out of the transcriber.
    const CpuReferenceModelData model{
        .kind = CpuReferenceModelKind::FixtureV1,
        .transcript = QStringLiteral("hello from fixture runtime"),
    };

    const std::vector<float> samples{0.1F, -0.1F, 0.05F, -0.05F};
    const std::optional<CpuDecodeResult> result = runCpuDecodePass(CpuDecodeRequest{
        .samples = samples,
        .model = &model,
        .sampleRate = 16000,
    });

    if (!result.has_value()) {
        QFAIL("Expected fixture decode result");
        return;
    }
    const CpuDecodeResult &decodeResult = result.value();
    QCOMPARE(decodeResult.transcript, QStringLiteral("hello from fixture runtime"));
    QCOMPARE(decodeResult.tokens.size(), static_cast<std::size_t>(4));
    QCOMPARE(static_cast<int>(decodeResult.tokens.front().kind), static_cast<int>(CpuDecodedTokenKind::Lexical));
    QCOMPARE(decodeResult.event.kind, TranscriptEventKind::Final);
    QCOMPARE(decodeResult.event.text, QStringLiteral("hello from fixture runtime"));
}

void CpuDecoderRuntimeTest::templateModelMatchesAudioProfile()
{
    // WHAT: Verify that the decoder runtime routes a template-backed native payload through feature extraction, search, and timestamp assembly.
    // HOW: Build an in-memory template model with two phrases, synthesize audio from the second phrase profile, and run one decode pass.
    // WHY: This keeps decoder-owned execution logic testable without involving package IO or the transcriber session wrapper.
    const std::vector<float> firstProfile =
        makeNormalizedProfile({0.05F, 0.12F, 0.28F, 0.60F, 0.85F, 0.55F, 0.32F, 0.18F, 0.12F, 0.09F, 0.05F, 0.03F});
    const std::vector<float> secondProfile =
        makeNormalizedProfile({0.82F, 0.72F, 0.18F, 0.10F, 0.12F, 0.16F, 0.44F, 0.78F, 0.64F, 0.22F, 0.10F, 0.06F});
    const CpuReferenceModelData model{
        .kind = CpuReferenceModelKind::TemplateDecoderScaffoldV2,
        .transcript = {},
        .featureBinCount = static_cast<int>(secondProfile.size()),
        .maxDistance = 0.35F,
        .phraseTemplates = {
            CpuDecodedPhraseTemplate{
                .text = QStringLiteral("hello from cpu runtime"),
                .featureProfile = firstProfile,
            },
            CpuDecodedPhraseTemplate{
                .text = QStringLiteral("open editor"),
                .featureProfile = secondProfile,
            },
        },
    };

    const std::vector<float> samples = synthesizeSamples(secondProfile);
    const std::optional<CpuDecodeResult> result = runCpuDecodePass(CpuDecodeRequest{
        .samples = samples,
        .model = &model,
        .sampleRate = 16000,
    });

    if (!result.has_value()) {
        QFAIL("Expected template decode result");
        return;
    }
    const CpuDecodeResult &decodeResult = result.value();
    QCOMPARE(decodeResult.transcript, QStringLiteral("open editor"));
    QCOMPARE(decodeResult.tokens.size(), static_cast<std::size_t>(2));
    QCOMPARE(decodeResult.event.text, QStringLiteral("open editor"));
    QVERIFY(decodeResult.event.endMs > 0);
}

void CpuDecoderRuntimeTest::timestampTokensDriveFinalEventRange()
{
    // WHAT: Verify that the native timestamp helper prefers decoder timestamp tokens when they are present.
    // HOW: Build a small execution-metadata block plus timestamp-bearing decoded tokens and synthesize one final event.
    // WHY: Phase 5B needs the native runtime to move away from pure utterance-duration timestamps toward token-driven timing.
    const CpuReferenceExecutionMetadata execution{
        .timestampMode = QStringLiteral("timestamp-token-v1"),
        .timestampTokenStartId = 100,
        .timestampTokenEndId = 199,
    };
    const std::vector<CpuDecodedToken> tokens{
        CpuDecodedToken{.id = 103, .text = {}, .kind = CpuDecodedTokenKind::Timestamp},
        CpuDecodedToken{.id = 110, .text = {}, .kind = CpuDecodedTokenKind::Timestamp},
    };
    const std::vector<float> samples(16000, 0.1F);

    const TranscriptEvent event = buildCpuFinalTranscriptEvent(QStringLiteral("hello"),
                                                               tokens,
                                                               samples,
                                                               16000,
                                                               &execution);

    QCOMPARE(event.startMs, 60);
    QCOMPARE(event.endMs, 220);
}

QTEST_APPLESS_MAIN(CpuDecoderRuntimeTest)

#include "cpudecoderruntimetest.moc"
