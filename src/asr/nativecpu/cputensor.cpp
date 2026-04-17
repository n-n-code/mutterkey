#include "asr/nativecpu/cputensor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

std::size_t CpuTensor::rowOffset(int r) const
{
    return static_cast<std::size_t>(r) * static_cast<std::size_t>(cols);
}

std::span<float> CpuTensor::row(int r)
{
    return std::span<float>(data).subspan(rowOffset(r), static_cast<std::size_t>(cols));
}

std::span<const float> CpuTensor::row(int r) const
{
    return std::span<const float>(data).subspan(rowOffset(r), static_cast<std::size_t>(cols));
}

CpuTensor::CpuTensor(int rows, int cols)
    : data(static_cast<std::size_t>(rows) * cols, 0.0F)
    , rows(rows)
    , cols(cols)
{
}

CpuTensor::CpuTensor(int rows, int cols, float fill)
    : data(static_cast<std::size_t>(rows) * cols, fill)
    , rows(rows)
    , cols(cols)
{
}

CpuTensor CpuTensor::columnSlice(int startCol, int colCount) const
{
    CpuTensor result(rows, colCount);
    for (int r = 0; r < rows; ++r) {
        const std::span<const float> src = row(r).subspan(static_cast<std::size_t>(startCol), static_cast<std::size_t>(colCount));
        std::ranges::copy(src, result.row(r).begin());
    }
    return result;
}

void CpuTensor::setColumnSlice(int startCol, const CpuTensor &src)
{
    for (int r = 0; r < rows && r < src.rows; ++r) {
        const std::span<const float> srcRow = src.row(r);
        const std::span<float> dstRow = row(r).subspan(static_cast<std::size_t>(startCol), static_cast<std::size_t>(src.cols));
        std::ranges::copy(srcRow, dstRow.begin());
    }
}

CpuTensor matmul(const CpuTensor &a, const CpuTensor &b)
{
    CpuTensor c(a.rows, b.cols);
    for (int i = 0; i < a.rows; ++i) {
        for (int k = 0; k < a.cols; ++k) {
            const float aVal = a.at(i, k);
            for (int j = 0; j < b.cols; ++j) {
                c.at(i, j) += aVal * b.at(k, j);
            }
        }
    }
    return c;
}

CpuTensor matmulTransposed(const CpuTensor &a, const CpuTensor &b)
{
    CpuTensor c(a.rows, b.rows);
    for (int i = 0; i < a.rows; ++i) {
        for (int j = 0; j < b.rows; ++j) {
            float dot = 0.0F;
            for (int k = 0; k < a.cols; ++k) {
                dot += a.at(i, k) * b.at(j, k);
            }
            c.at(i, j) = dot;
        }
    }
    return c;
}

void addBiasInPlace(CpuTensor &a, const CpuTensor &bias)
{
    for (int r = 0; r < a.rows; ++r) {
        for (int c = 0; c < a.cols; ++c) {
            a.at(r, c) += bias.at(0, c);
        }
    }
}

void addInPlace(CpuTensor &a, const CpuTensor &b)
{
    const std::size_t n = a.data.size();
    for (std::size_t i = 0; i < n; ++i) {
        a.data.at(i) += b.data.at(i);
    }
}

void layerNormInPlace(CpuTensor &x, const CpuTensor &gamma, const CpuTensor &beta, float eps)
{
    for (int r = 0; r < x.rows; ++r) {
        const int n = x.cols;

        float mean = 0.0F;
        for (int c = 0; c < n; ++c) {
            mean += x.at(r, c);
        }
        mean /= static_cast<float>(n);

        float variance = 0.0F;
        for (int c = 0; c < n; ++c) {
            const float d = x.at(r, c) - mean;
            variance += d * d;
        }
        variance /= static_cast<float>(n);

        const float invStd = 1.0F / std::sqrt(variance + eps);
        for (int c = 0; c < n; ++c) {
            x.at(r, c) = ((x.at(r, c) - mean) * invStd * gamma.at(0, c)) + beta.at(0, c);
        }
    }
}

void geluInPlace(CpuTensor &x)
{
    // Tanh approximation: GELU(x) = 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
    constexpr float kSqrt2OverPi = 0.7978845608028654F;
    constexpr float kCoeff = 0.044715F;
    for (float &v : x.data) {
        const float inner = kSqrt2OverPi * (v + (kCoeff * v * v * v));
        v = 0.5F * v * (1.0F + std::tanh(inner));
    }
}

void softmaxRowsInPlace(CpuTensor &x)
{
    for (int r = 0; r < x.rows; ++r) {
        const int n = x.cols;

        float maxVal = -std::numeric_limits<float>::infinity();
        for (int c = 0; c < n; ++c) {
            maxVal = std::max(maxVal, x.at(r, c));
        }

        float sum = 0.0F;
        for (int c = 0; c < n; ++c) {
            x.at(r, c) = std::exp(x.at(r, c) - maxVal);
            sum += x.at(r, c);
        }

        if (sum > 0.0F) {
            const float invSum = 1.0F / sum;
            for (int c = 0; c < n; ++c) {
                x.at(r, c) *= invSum;
            }
        }
    }
}

void scaleInPlace(CpuTensor &x, float factor)
{
    for (float &v : x.data) {
        v *= factor;
    }
}

void applyCausalMaskInPlace(CpuTensor &x, int maskFromCol)
{
    constexpr float kNegInf = -std::numeric_limits<float>::infinity();
    for (int r = 0; r < x.rows; ++r) {
        for (int c = maskFromCol; c < x.cols; ++c) {
            x.at(r, c) = kNegInf;
        }
    }
}

int argmax(std::span<const float> values)
{
    if (values.empty()) {
        return -1;
    }
    int bestIndex = 0;
    float bestValue = values.front();
    for (std::size_t i = 1; i < values.size(); ++i) {
        if (values.subspan(i, 1).front() > bestValue) {
            bestValue = values.subspan(i, 1).front();
            bestIndex = static_cast<int>(i);
        }
    }
    return bestIndex;
}

CpuTensor conv1d(const CpuConv1dRequest &request)
{
    const int inTime = request.input.rows;
    const int inChannels = request.input.cols;
    const int outChannels = request.weight.rows;
    const int outTime = (((inTime + (2 * request.padding)) - request.kernelSize) / request.stride) + 1;

    CpuTensor output(outTime, outChannels);

    // PyTorch Conv1d stores weight as [out, in, kernel] row-major; after the
    // converter flattens to [out, in*kernel] the column index for (ic, k) is
    // ic*kernelSize + k.
    for (int oc = 0; oc < outChannels; ++oc) {
        const float biasVal = request.bias.at(0, oc);
        for (int t = 0; t < outTime; ++t) {
            float sum = biasVal;
            const int tStart = (t * request.stride) - request.padding;
            for (int k = 0; k < request.kernelSize; ++k) {
                const int tIn = tStart + k;
                if (tIn < 0 || tIn >= inTime) {
                    continue;
                }
                for (int ic = 0; ic < inChannels; ++ic) {
                    const int weightCol = (ic * request.kernelSize) + k;
                    sum += request.input.at(tIn, ic) * request.weight.at(oc, weightCol);
                }
            }
            output.at(t, oc) = sum;
        }
    }
    return output;
}
