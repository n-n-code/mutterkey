#pragma once

#include <QString>

#include <optional>
#include <vector>

/**
 * @file
 * @brief Minimal test-only PCM WAV reader for conformance fixtures.
 *
 * Only supports 16 kHz, mono, 16-bit PCM — the format used by the committed
 * `third_party/whisper.cpp/samples/jfk.wav` fixture. Test code should not use
 * this helper for arbitrary user input.
 */

/**
 * @brief Read a mono 16-bit PCM WAV at 16 kHz into normalized float samples.
 * @param path Filesystem path to the WAV file.
 * @param error Optional output populated with a human-readable error on failure.
 * @return Samples scaled to `[-1.0F, 1.0F]` on success.
 */
[[nodiscard]] std::optional<std::vector<float>> readMono16kHzPcmWav(const QString &path, QString *error = nullptr);
