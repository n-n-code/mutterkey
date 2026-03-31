#pragma once

#include <QString>
#include <QStringList>

#include <cstdint>
#include <span>
#include <vector>

struct CpuReferenceExecutionMetadata;

/**
 * @file
 * @brief Small tokenizer helpers for the native CPU runtime.
 */
/**
 * @brief Category assigned to one decoded token emitted by the native CPU runtime.
 */
enum class CpuDecodedTokenKind : std::uint8_t {
    Lexical,
    Timestamp,
    Control,
};

/**
 * @brief One decoded token emitted by a native CPU search policy.
 */
struct CpuDecodedToken {
    /// Stable token id within the current simple tokenizer contract.
    int id = 0;
    /// Human-readable token text.
    QString text;
    /// Decoder-owned token category used by downstream runtime logic.
    CpuDecodedTokenKind kind = CpuDecodedTokenKind::Lexical;
};

/**
 * @brief Tokenizes a final transcript into stable simple decoder tokens.
 * @param text Final transcript text to tokenize.
 * @return Decoder tokens suitable for intermediate search/result plumbing.
 */
[[nodiscard]] std::vector<CpuDecodedToken> tokenizeCpuTranscript(QStringView text);

/**
 * @brief Tokenizes a final transcript using a packaged vocabulary for stable ids when available.
 * @param text Final transcript text to tokenize.
 * @param vocabulary Optional packaged vocabulary used to resolve stable token ids.
 * @return Decoder tokens suitable for intermediate search/result plumbing.
 */
[[nodiscard]] std::vector<CpuDecodedToken> tokenizeCpuTranscript(QStringView text, std::span<const QString> vocabulary);

/**
 * @brief Classifies decoded tokens into lexical, control, or timestamp categories.
 * @param tokens Token sequence to classify in place.
 * @param execution Native execution metadata providing special-token ranges.
 */
void classifyCpuDecodedTokens(std::vector<CpuDecodedToken> *tokens, const CpuReferenceExecutionMetadata &execution);
