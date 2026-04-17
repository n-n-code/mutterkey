#include "asr/nativecpu/cputensor.h"

#include <QtTest/QTest>

#include <cmath>

namespace {

class CpuTensorTest final : public QObject
{
    Q_OBJECT

private slots:
    void conv1dUsesPyTorchFlattenedWeightLayout();
};

void compareClose(float actual, float expected)
{
    QVERIFY(std::fabs(actual - expected) < 1e-5F);
}

} // namespace

void CpuTensorTest::conv1dUsesPyTorchFlattenedWeightLayout()
{
    // WHAT: Verify native Conv1d reads flattened PyTorch weights as [out, in, kernel].
    // HOW: Convolve a tiny two-channel sequence with hand-computed weights whose result changes if kernel/in-channel order is swapped.
    // WHY: Real Whisper encoder conv weights are flattened from PyTorch tensors; a layout regression breaks real decoding before attention runs.
    CpuTensor input(3, 2);
    input.at(0, 0) = 1.0F;
    input.at(0, 1) = 10.0F;
    input.at(1, 0) = 2.0F;
    input.at(1, 1) = 20.0F;
    input.at(2, 0) = 3.0F;
    input.at(2, 1) = 30.0F;

    CpuTensor weight(1, 4);
    weight.at(0, 0) = 100.0F; // input channel 0, kernel 0
    weight.at(0, 1) = 1.0F;   // input channel 0, kernel 1
    weight.at(0, 2) = 10.0F;  // input channel 1, kernel 0
    weight.at(0, 3) = 0.1F;   // input channel 1, kernel 1

    const CpuTensor bias(1, 1, 0.5F);
    const CpuTensor output = conv1d(CpuConv1dRequest{
        .input = input,
        .weight = weight,
        .bias = bias,
        .kernelSize = 2,
        .stride = 1,
        .padding = 0,
    });

    QCOMPARE(output.rows, 2);
    QCOMPARE(output.cols, 1);
    compareClose(output.at(0, 0), 204.5F);
    compareClose(output.at(1, 0), 406.5F);
}

QTEST_APPLESS_MAIN(CpuTensorTest)

#include "cputensortest.moc"
