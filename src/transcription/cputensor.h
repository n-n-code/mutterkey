#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

/**
 * @file
 * @brief Minimal inference-only tensor types and operations for the native CPU runtime.
 *
 * All tensors are 2D row-major float32. Operations are free functions with no
 * dynamic dispatch, no computation graph, and no autograd. Designed for the
 * small dimensions of the Whisper base.en model family.
 */

/**
 * @brief Owned 2D row-major float32 tensor used for activations and weights.
 */
struct CpuTensor {
    /// Contiguous row-major storage.
    std::vector<float> data;
    /// Number of rows.
    int rows = 0;
    /// Number of columns.
    int cols = 0;

    CpuTensor() = default;

    /**
     * @brief Constructs a zero-filled tensor.
     * @param rows Number of rows.
     * @param cols Number of columns.
     */
    CpuTensor(int rows, int cols);

    /**
     * @brief Constructs a tensor filled with a constant value.
     * @param rows Number of rows.
     * @param cols Number of columns.
     * @param fill Fill value for every element.
     */
    CpuTensor(int rows, int cols, float fill);

    /**
     * @brief Returns whether the tensor storage is empty.
     * @return `true` when no elements are stored.
     */
    [[nodiscard]] bool isEmpty() const { return data.empty(); }

    /**
     * @brief Returns the logical element count.
     * @return `rows * cols`.
     */
    [[nodiscard]] int elementCount() const { return rows * cols; }

    /**
     * @brief Returns the starting offset of a row in the flat storage.
     * @param r Row index.
     * @return Flat element offset for the row.
     */
    [[nodiscard]] std::size_t rowOffset(int r) const;

    /**
     * @brief Returns a mutable pointer to the first element of a row.
     * @param r Row index.
     * @return Pointer to the row storage.
     */
    [[nodiscard]] float *rowPtr(int r) { return row(r).data(); }

    /**
     * @brief Returns a const pointer to the first element of a row.
     * @param r Row index.
     * @return Pointer to the row storage.
     */
    [[nodiscard]] const float *rowPtr(int r) const { return row(r).data(); }

    /**
     * @brief Returns a mutable span over a row.
     * @param r Row index.
     * @return Span covering the row elements.
     */
    [[nodiscard]] std::span<float> row(int r);

    /**
     * @brief Returns a const span over a row.
     * @param r Row index.
     * @return Span covering the row elements.
     */
    [[nodiscard]] std::span<const float> row(int r) const;

    /**
     * @brief Returns a mutable reference to an element.
     * @param r Row index.
     * @param c Column index.
     * @return Reference to the requested element.
     */
    [[nodiscard]] float &at(int r, int c) { return data.at(rowOffset(r) + static_cast<std::size_t>(c)); }

    /**
     * @brief Returns an element by value.
     * @param r Row index.
     * @param c Column index.
     * @return Value of the requested element.
     */
    [[nodiscard]] float at(int r, int c) const { return data.at(rowOffset(r) + static_cast<std::size_t>(c)); }

    /**
     * @brief Extracts a contiguous column range into a new tensor.
     * @param startCol First column to include.
     * @param colCount Number of columns to extract.
     * @return New tensor of shape (rows, colCount).
     */
    [[nodiscard]] CpuTensor columnSlice(int startCol, int colCount) const;

    /**
     * @brief Writes columns from @p src into this tensor starting at @p startCol.
     * @param startCol Destination start column.
     * @param src Source tensor whose columns are copied into this tensor.
     */
    void setColumnSlice(int startCol, const CpuTensor &src);
};

/**
 * @brief Regular matrix multiply: C(m,n) = A(m,k) * B(k,n).
 * @param a Left operand of shape (m, k).
 * @param b Right operand of shape (k, n).
 * @return Product tensor of shape (m, n).
 */
[[nodiscard]] CpuTensor matmul(const CpuTensor &a, const CpuTensor &b);

/**
 * @brief Transposed matrix multiply: C(m,n) = A(m,k) * B(n,k)^T.
 *
 * Primary operation for linear projections where weights are stored as
 * (out_features, in_features) and the computation is input @ weight^T.
 * Both operands are accessed row-wise for good cache behavior.
 * @param a Left operand of shape (m, k).
 * @param b Right operand of shape (n, k), interpreted as transposed.
 * @return Product tensor of shape (m, n).
 */
[[nodiscard]] CpuTensor matmulTransposed(const CpuTensor &a, const CpuTensor &b);

/**
 * @brief Adds a 1D bias vector to each row of @p a in place.
 * @param a Tensor updated in place.
 * @param bias Single-row tensor of shape (1, a.cols).
 */
void addBiasInPlace(CpuTensor &a, const CpuTensor &bias);

/**
 * @brief Element-wise addition: a += b. Both must have the same shape.
 * @param a Destination tensor updated in place.
 * @param b Source tensor added element-wise.
 */
void addInPlace(CpuTensor &a, const CpuTensor &b);

/**
 * @brief Per-row layer normalization in place.
 * @param x Tensor updated in place.
 * @param gamma Scale tensor of shape (1, x.cols).
 * @param beta Shift tensor of shape (1, x.cols).
 * @param eps Small constant for numerical stability.
 */
void layerNormInPlace(CpuTensor &x, const CpuTensor &gamma, const CpuTensor &beta, float eps = 1e-5F);

/**
 * @brief GELU activation in place (tanh approximation used by Whisper).
 * @param x Tensor updated in place.
 */
void geluInPlace(CpuTensor &x);

/**
 * @brief Per-row softmax in place.
 * @param x Tensor updated in place.
 */
void softmaxRowsInPlace(CpuTensor &x);

/**
 * @brief Scalar multiplication in place.
 * @param x Tensor updated in place.
 * @param factor Scalar multiplier.
 */
void scaleInPlace(CpuTensor &x, float factor);

/**
 * @brief Applies a causal mask: sets x[r][c] = -infinity for c >= maskFromCol.
 *
 * Used in decoder self-attention to prevent attending to future positions.
 * @param x Tensor updated in place.
 * @param maskFromCol First disallowed column index.
 */
void applyCausalMaskInPlace(CpuTensor &x, int maskFromCol);

/**
 * @brief Returns the index of the maximum element.
 * @param values Span to scan.
 * @return Index of the largest element.
 */
[[nodiscard]] int argmax(std::span<const float> values);

/**
 * @brief Parameters for a 1D convolution over a time-major tensor.
 */
struct CpuConv1dRequest {
    /// Input tensor of shape (time, inChannels).
    const CpuTensor &input;
    /// Kernel tensor of shape (outChannels, inChannels * kernelSize).
    const CpuTensor &weight;
    /// Bias tensor of shape (1, outChannels).
    const CpuTensor &bias;
    /// Convolution kernel width.
    int kernelSize = 0;
    /// Convolution stride.
    int stride = 1;
    /// Zero-padding applied to both sides of the time axis.
    int padding = 0;
};

/**
 * @brief Runs a 1D convolution: input(time, inCh) -> output(outTime, outCh).
 *
 * @param request Convolution input, weights, bias, and shape parameters.
 * @return Convolved output tensor.
 */
[[nodiscard]] CpuTensor conv1d(const CpuConv1dRequest &request);
