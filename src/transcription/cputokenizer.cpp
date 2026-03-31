#include "transcription/cputokenizer.h"

#include "transcription/cpureferencemodel.h"

#include <algorithm>
#include <QRegularExpression>

std::vector<CpuDecodedToken> tokenizeCpuTranscript(QStringView text)
{
    return tokenizeCpuTranscript(text, {});
}

std::vector<CpuDecodedToken> tokenizeCpuTranscript(QStringView text, std::span<const QString> vocabulary)
{
    static const QRegularExpression kWhitespacePattern(QStringLiteral("\\s+"));

    const QString normalized = text.toString().simplified();
    if (normalized.isEmpty()) {
        return {};
    }

    const QStringList parts = normalized.split(kWhitespacePattern, Qt::SkipEmptyParts);
    std::vector<CpuDecodedToken> tokens;
    tokens.reserve(static_cast<std::size_t>(parts.size()));
    for (int index = 0; index < parts.size(); ++index) {
        int tokenId = index + 1;
        if (!vocabulary.empty()) {
            const auto it = std::ranges::find(vocabulary, parts.at(index));
            if (it != vocabulary.end()) {
                tokenId = static_cast<int>(std::distance(vocabulary.begin(), it));
            }
        }

        tokens.push_back(CpuDecodedToken{
            .id = tokenId,
            .text = parts.at(index),
            .kind = CpuDecodedTokenKind::Lexical,
        });
    }
    return tokens;
}

void classifyCpuDecodedTokens(std::vector<CpuDecodedToken> *tokens, const CpuReferenceExecutionMetadata &execution)
{
    if (tokens == nullptr) {
        return;
    }

    for (CpuDecodedToken &token : *tokens) {
        if (token.id >= execution.timestampTokenStartId && token.id <= execution.timestampTokenEndId) {
            token.kind = CpuDecodedTokenKind::Timestamp;
            continue;
        }
        if (token.id == execution.bosTokenId || token.id == execution.eosTokenId || token.id == execution.noSpeechTokenId) {
            token.kind = CpuDecodedTokenKind::Control;
            continue;
        }
        token.kind = CpuDecodedTokenKind::Lexical;
    }
}
