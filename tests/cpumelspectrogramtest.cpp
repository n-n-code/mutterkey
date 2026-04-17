#include "asr/nativecpu/cpumelspectrogram.h"

#include <QtTest/QTest>

#include <cmath>
#include <span>

namespace {

class CpuMelSpectrogramTest final : public QObject
{
    Q_OBJECT

private slots:
    void computesSlaneyNormalizedMelFilterBank();
    void extractsLog10NormalizedSpectrogram();
};

void compareClose(float actual, float expected, float tolerance = 1e-6F)
{
    QVERIFY(std::fabs(actual - expected) <= tolerance);
}

float filterAt(const CpuMelFilterBank &filterBank, int melBand, int fftBin)
{
    return filterBank.filters.at((static_cast<std::size_t>(melBand) * static_cast<std::size_t>(filterBank.fftBins))
                                 + static_cast<std::size_t>(fftBin));
}

float rowSum(const CpuMelFilterBank &filterBank, int melBand)
{
    float sum = 0.0F;
    for (int fftBin = 0; fftBin < filterBank.fftBins; ++fftBin) {
        sum += filterAt(filterBank, melBand, fftBin);
    }
    return sum;
}

} // namespace

void CpuMelSpectrogramTest::computesSlaneyNormalizedMelFilterBank()
{
    // WHAT: Verify the synthesized fallback mel filterbank matches Slaney/librosa-style coefficients.
    // HOW: Compute a small four-band 16 kHz / 512-point bank and compare selected golden coefficients and row area.
    // WHY: Whisper frontend parity depends on Slaney scale plus area normalization; HTK-scale regressions break real decoder quality.
    const CpuMelFilterBank filterBank = computeMelFilterBank(CpuMelFilterBankSpec{
        .sampleRate = 16000,
        .fftSize = 512,
        .melBands = 4,
    });

    QCOMPARE(filterBank.melBands, 4);
    QCOMPARE(filterBank.fftBins, 257);
    compareClose(filterAt(filterBank, 0, 10), 0.0008372501F);
    compareClose(filterAt(filterBank, 1, 40), 0.0011612914F);
    compareClose(filterAt(filterBank, 2, 80), 0.0005901678F);
    compareClose(filterAt(filterBank, 3, 160), 0.0002843184F);
    compareClose(rowSum(filterBank, 0), 0.031992274F, 1e-7F);
    compareClose(rowSum(filterBank, 3), 0.031999463F, 1e-7F);
}

void CpuMelSpectrogramTest::extractsLog10NormalizedSpectrogram()
{
    // WHAT: Verify log-mel extraction uses Whisper's log10 and `(x + 4) / 4` normalization.
    // HOW: Feed one four-sample Hann-windowed frame through a one-bin filterbank with hand-computed power.
    // WHY: The real decoder is sensitive to frontend scaling, so this cheap test protects the normalization contract outside the env-gated model run.
    const std::vector<float> samples{1.0F, 1.0F, 1.0F, 1.0F};
    const CpuMelFilterBank filterBank{
        .filters = {1.0F, 0.0F, 0.0F},
        .melBands = 1,
        .fftBins = 3,
    };

    const CpuTensor mel = extractLogMelSpectrogram(std::span<const float>(samples), filterBank, CpuMelConfig{
        .sampleRate = 16000,
        .fftSize = 4,
        .hopLength = 4,
        .melBands = 1,
        .maxFrames = 2,
    });

    QCOMPARE(mel.rows, 1);
    QCOMPARE(mel.cols, 2);
    compareClose(mel.at(0, 0), 1.150515F);
    compareClose(mel.at(0, 1), -0.849485F);
}

QTEST_APPLESS_MAIN(CpuMelSpectrogramTest)

#include "cpumelspectrogramtest.moc"
