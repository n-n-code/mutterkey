#include "asr/nativecpu/cpuwhispertokenizer.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include <limits>
#include <utility>

namespace {

RuntimeError makeRuntimeError(RuntimeErrorCode code, QString message, QString detail = {})
{
    return RuntimeError{.code = code, .message = std::move(message), .detail = std::move(detail)};
}

QString readTrimmedTextFile(const QString &path, RuntimeError *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::ModelLoadFailed,
                                      QStringLiteral("Failed to open native CPU tokenizer asset"),
                                      QFileInfo(path).absoluteFilePath());
        }
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

QString mergeKey(QStringView left, QStringView right)
{
    QString key;
    key.reserve(left.size() + right.size() + 1);
    key.append(left);
    key.append(QChar::fromLatin1('\x1f'));
    key.append(right);
    return key;
}

std::vector<QString> bpeEncodePiece(const QString &piece, const CpuWhisperTokenizerModel &model)
{
    std::vector<QString> symbols;
    symbols.reserve(static_cast<std::size_t>(piece.size()));
    for (const QChar character : piece) {
        symbols.emplace_back(character);
    }

    if (symbols.empty()) {
        return symbols;
    }

    while (symbols.size() > 1U) {
        int bestIndex = -1;
        int bestRank = std::numeric_limits<int>::max();

        for (int index = 0; index + 1 < static_cast<int>(symbols.size()); ++index) {
            const auto leftIndex = static_cast<std::size_t>(index);
            const auto rightIndex = leftIndex + 1U;
            const auto it = model.mergeRanks.constFind(mergeKey(symbols.at(leftIndex), symbols.at(rightIndex)));
            if (it != model.mergeRanks.cend() && *it < bestRank) {
                bestRank = *it;
                bestIndex = index;
            }
        }

        if (bestIndex < 0) {
            break;
        }

        const auto leftIndex = static_cast<std::size_t>(bestIndex);
        const auto rightIndex = leftIndex + 1U;
        symbols.at(leftIndex) = symbols.at(leftIndex) + symbols.at(rightIndex);
        symbols.erase(symbols.begin() + bestIndex + 1);
    }

    return symbols;
}

int tokenIdForPiece(QStringView piece, const CpuWhisperTokenizerModel &model)
{
    const auto it = model.tokenIds.constFind(piece.toString());
    return it != model.tokenIds.cend() ? *it : -1;
}

} // namespace

std::optional<CpuWhisperTokenizerModel>
loadCpuWhisperTokenizerModel(const QString &vocabularyPath, const QString &mergesPath, RuntimeError *error)
{
    RuntimeError runtimeError;
    RuntimeError *activeError = error != nullptr ? error : &runtimeError;

    const QString vocabularyText = readTrimmedTextFile(vocabularyPath, activeError);
    if (!activeError->isOk()) {
        return std::nullopt;
    }

    const QString mergesText = readTrimmedTextFile(mergesPath, activeError);
    if (!activeError->isOk()) {
        return std::nullopt;
    }

    CpuWhisperTokenizerModel model;
    const QStringList vocabularyLines = vocabularyText.split(QLatin1Char('\n'));
    model.vocabulary.reserve(static_cast<std::size_t>(vocabularyLines.size()));
    for (const QString &line : vocabularyLines) {
        QString token = line;
        if (token.endsWith(QLatin1Char('\r'))) {
            token.chop(1);
        }
        if (token.isEmpty()) {
            continue;
        }
        const int id = static_cast<int>(model.vocabulary.size());
        model.tokenIds.insert(token, id);
        model.vocabulary.push_back(token);
    }

    const QStringList mergeLines = mergesText.split(QLatin1Char('\n'));
    model.merges.reserve(static_cast<std::size_t>(mergeLines.size()));
    int rank = 0;
    for (const QString &rawLine : mergeLines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() != 2) {
            if (error != nullptr) {
                *error = makeRuntimeError(RuntimeErrorCode::InvalidModelPackage,
                                          QStringLiteral("Native CPU tokenizer merge file is malformed"),
                                          QFileInfo(mergesPath).absoluteFilePath());
            }
            return std::nullopt;
        }

        model.merges.push_back(CpuWhisperMergeRule{
            .left = parts.at(0),
            .right = parts.at(1),
        });
        model.mergeRanks.insert(mergeKey(parts.at(0), parts.at(1)), rank);
        ++rank;
    }

    if (model.vocabulary.empty()) {
        if (error != nullptr) {
            *error = makeRuntimeError(RuntimeErrorCode::InvalidModelPackage,
                                      QStringLiteral("Native CPU tokenizer vocabulary is empty"),
                                      QFileInfo(vocabularyPath).absoluteFilePath());
        }
        return std::nullopt;
    }

    return model;
}

std::vector<CpuDecodedToken>
tokenizeCpuTranscriptWhisper(QStringView text, const CpuWhisperTokenizerModel &model)
{
    const QString normalized = text.toString().simplified();
    if (normalized.isEmpty()) {
        return {};
    }

    const QStringList words = normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    std::vector<CpuDecodedToken> tokens;

    for (const QString &piece : words) {
        const std::vector<QString> symbols = bpeEncodePiece(piece, model);
        for (const QString &symbol : symbols) {
            const int tokenId = tokenIdForPiece(symbol, model);
            if (tokenId >= 0) {
                tokens.push_back(CpuDecodedToken{
                    .id = tokenId,
                    .text = symbol,
                    .kind = CpuDecodedTokenKind::Lexical,
                });
                continue;
            }

            for (const QChar character : symbol) {
                const QString singleCharacter(character);
                tokens.push_back(CpuDecodedToken{
                    .id = tokenIdForPiece(singleCharacter, model),
                    .text = singleCharacter,
                    .kind = CpuDecodedTokenKind::Lexical,
                });
            }
        }
    }

    return tokens;
}
