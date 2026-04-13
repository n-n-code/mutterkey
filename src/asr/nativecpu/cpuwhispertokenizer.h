#pragma once

#include "asr/nativecpu/cputokenizer.h"
#include "asr/runtime/transcriptiontypes.h"

#include <QHash>
#include <QString>
#include <QStringView>

#include <optional>
#include <vector>

/**
 * @file
 * @brief Whisper-family tokenizer helpers for native CPU decoder packages.
 */

/**
 * @brief One merge rule in the packaged BPE tokenizer.
 */
struct CpuWhisperMergeRule {
    /// Left symbol in the merge pair.
    QString left;
    /// Right symbol in the merge pair.
    QString right;
};

/**
 * @brief Immutable packaged Whisper-family tokenizer model.
 */
struct CpuWhisperTokenizerModel {
    /// Ordered vocabulary entries loaded from the packaged tokenizer asset.
    std::vector<QString> vocabulary;
    /// Ordered BPE merge rules loaded from the packaged merge asset.
    std::vector<CpuWhisperMergeRule> merges;
    /// Lookup table from token text to token id.
    QHash<QString, int> tokenIds;
    /// Lookup table from merge pair to merge priority rank.
    QHash<QString, int> mergeRanks;
};

/**
 * @brief Loads a packaged Whisper-family tokenizer from vocab and merge assets.
 * @param vocabularyPath Line-oriented vocabulary asset path.
 * @param mergesPath Line-oriented merge-rules asset path.
 * @param error Optional destination for load failures.
 * @return Loaded tokenizer model on success.
 */
[[nodiscard]] std::optional<CpuWhisperTokenizerModel>
loadCpuWhisperTokenizerModel(const QString &vocabularyPath, const QString &mergesPath, RuntimeError *error = nullptr);

/**
 * @brief Tokenizes transcript text with the packaged Whisper-family tokenizer.
 * @param text Transcript text to tokenize.
 * @param model Loaded tokenizer model.
 * @return Stable decoder tokens derived from the packaged BPE assets.
 */
[[nodiscard]] std::vector<CpuDecodedToken>
tokenizeCpuTranscriptWhisper(QStringView text, const CpuWhisperTokenizerModel &model);
