#pragma once

#include "transcription/cpumodelweights.h"
#include "transcription/cputensor.h"

/**
 * @file
 * @brief Encoder forward pass for the native CPU Whisper runtime.
 *
 * Implements the Whisper encoder: two conv1d layers with GELU, sinusoidal
 * positional encoding, and a stack of pre-norm transformer layers with
 * multi-head self-attention and feed-forward networks.
 */

/**
 * @brief Runs the full encoder forward pass.
 * @param mel Log-mel spectrogram of shape (melBands, frames).
 * @param weights Loaded encoder weights.
 * @param config Model hyperparameters.
 * @return Encoder output of shape (encoderPositions, audioState).
 */
[[nodiscard]] CpuTensor runEncoderForward(const CpuTensor &mel,
                                           const CpuEncoderWeights &weights,
                                           const CpuModelConfig &config);
