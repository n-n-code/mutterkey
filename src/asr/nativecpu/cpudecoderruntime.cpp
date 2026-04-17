#include "asr/nativecpu/cpudecoderruntime.h"

#include "asr/nativecpu/cpuencoderforward.h"
#include "asr/nativecpu/cpufeatureextractor.h"
#include "asr/nativecpu/cpugreedysearch.h"
#include "asr/nativecpu/cpumelspectrogram.h"
#include "asr/nativecpu/cpumodelweights.h"
#include "asr/nativecpu/cputimestamps.h"
#include "asr/nativecpu/cputokensearch.h"
#include "asr/nativecpu/cpuwhispertokenizer.h"

#include <QStringList>
#include <algorithm>
#include <utility>

namespace {

CpuGreedySearchConfig realDecoderSearchConfig(const CpuModelConfig &config,
                                              const CpuReferenceExecutionMetadata *execution,
                                              int maxDecoderTokens)
{
    CpuGreedySearchConfig searchConfig;
    if (execution != nullptr) {
        searchConfig.sotTokenId = execution->bosTokenId;
        searchConfig.eotTokenId = execution->eosTokenId;
        searchConfig.noSpeechTokenId = execution->noSpeechTokenId;
        searchConfig.timestampBeginId = execution->timestampTokenStartId;
        searchConfig.timestampEndId = execution->timestampTokenEndId;
        searchConfig.initialPromptTokenIds = execution->initialPromptTokenIds;
        searchConfig.suppressedTokenIds = execution->suppressedTokenIds;
    }

    // Prompt footprint: one SOT token plus either the packaged prompt sequence
    // or the Whisper-en three-token default (language + transcribe + no_timestamps).
    const int packagedPromptTokenCount = static_cast<int>(searchConfig.initialPromptTokenIds.size());
    const int initialPromptTokenCount = 1 + (packagedPromptTokenCount > 0 ? packagedPromptTokenCount : 3);
    const int availableGeneratedTokens = std::max(0, config.textContextSize - initialPromptTokenCount);
    searchConfig.maxDecoderTokens = std::min(searchConfig.maxDecoderTokens, availableGeneratedTokens);
    if (maxDecoderTokens > 0) {
        searchConfig.maxDecoderTokens = std::min(searchConfig.maxDecoderTokens, maxDecoderTokens);
    }
    return searchConfig;
}

} // namespace

std::optional<CpuDecodeResult> runCpuDecodePass(const CpuDecodeRequest &request)
{
    if (request.model == nullptr || request.samples.empty() || request.sampleRate <= 0) {
        return std::nullopt;
    }

    QString transcript;
    std::vector<CpuDecodedToken> tokens;

    if (request.model->kind == CpuReferenceModelKind::FixtureV1) {
        transcript = request.model->transcript.trimmed();
        tokens = request.model->whisperTokenizer.has_value()
            ? tokenizeCpuTranscriptWhisper(transcript, *request.model->whisperTokenizer)
            : tokenizeCpuTranscript(transcript, request.model->tokenVocabulary);
    } else if (request.model->kind == CpuReferenceModelKind::TemplateDecoderScaffoldV2) {
        const std::vector<float> features = extractCpuEnergyProfile(request.samples, request.model->featureBinCount);
        const CpuTokenSearchResult searchResult = runCpuGreedyTokenSearch(CpuTokenSearchRequest{
            .features = features,
            .phraseTemplates = request.model->phraseTemplates,
            .maxDistance = request.model->maxDistance,
        });
        if (!searchResult.matched) {
            return std::nullopt;
        }

        transcript = searchResult.transcript.trimmed();
        tokens = request.model->whisperTokenizer.has_value()
            ? tokenizeCpuTranscriptWhisper(transcript, *request.model->whisperTokenizer)
            : tokenizeCpuTranscript(transcript, request.model->tokenVocabulary);
    } else if (request.model->kind == CpuReferenceModelKind::RealDecoderV3) {
        if (request.model->tensorWeights == nullptr || request.kvCache == nullptr) {
            return std::nullopt;
        }

        const CpuWhisperModelWeights &weights = *request.model->tensorWeights;
        const int maxMelFrames = request.maxMelFrames > 0 ? std::min(request.maxMelFrames, 3000) : 3000;
        const CpuMelConfig melConfig{
            .sampleRate = request.sampleRate,
            .fftSize = 400,
            .hopLength = 160,
            .melBands = weights.config.melBands,
            .maxFrames = maxMelFrames,
        };

        const CpuTensor mel = extractLogMelSpectrogram(request.samples, weights.melFilters, melConfig);
        const CpuTensor encoderOutput = runEncoderForward(mel, weights.encoder, weights.config);

        request.kvCache->reset();
        const CpuGreedySearchConfig searchConfig =
            realDecoderSearchConfig(weights.config, request.execution, request.maxDecoderTokens);

        CpuGreedySearchResult searchResult = runCpuGreedySearch(encoderOutput, weights, request.kvCache, searchConfig);
        if (!searchResult.isSpeech) {
            return std::nullopt;
        }

        // Resolve token text from the loaded tokenizer vocabulary.
        if (request.model->whisperTokenizer.has_value()) {
            const CpuWhisperTokenizerModel &tokenizerModel = *request.model->whisperTokenizer;
            QStringList textParts;
            for (CpuDecodedToken &token : searchResult.tokens) {
                if (token.kind == CpuDecodedTokenKind::Lexical && token.id >= 0
                    && std::cmp_less(token.id, tokenizerModel.vocabulary.size())) {
                    token.text = decodeCpuWhisperTokenText(tokenizerModel.vocabulary.at(static_cast<std::size_t>(token.id)));
                    textParts.append(token.text);
                }
            }
            transcript = textParts.join(QString()).trimmed();
        }
        tokens = std::move(searchResult.tokens);
    } else {
        const std::vector<float> features = extractCpuEnergyProfile(request.samples, request.model->featureBinCount);
        const CpuTokenSearchResult searchResult = runCpuGreedyTokenSearch(CpuTokenSearchRequest{
            .features = features,
            .phraseTemplates = request.model->phraseTemplates,
            .maxDistance = request.model->maxDistance,
        });
        if (!searchResult.matched || !request.model->whisperTokenizer.has_value()) {
            return std::nullopt;
        }

        transcript = searchResult.transcript.trimmed();
        tokens = tokenizeCpuTranscriptWhisper(transcript, *request.model->whisperTokenizer);
    }

    if (transcript.isEmpty()) {
        return std::nullopt;
    }

    if (request.execution != nullptr) {
        classifyCpuDecodedTokens(&tokens, *request.execution);
    }

    const TranscriptEvent event = buildCpuFinalTranscriptEvent(transcript,
                                                               tokens,
                                                               request.samples,
                                                               request.sampleRate,
                                                               request.execution);

    return CpuDecodeResult{
        .transcript = transcript,
        .tokens = std::move(tokens),
        .event = event,
    };
}
