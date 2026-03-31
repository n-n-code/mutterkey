#include "transcription/cpusessionstate.h"

void CpuSessionState::appendChunk(const AudioChunk &chunk)
{
    m_bufferedSamples.insert(m_bufferedSamples.end(), chunk.samples.begin(), chunk.samples.end());
}

const std::vector<float> &CpuSessionState::bufferedSamples() const
{
    return m_bufferedSamples;
}

bool CpuSessionState::isWarmedUp() const
{
    return m_warmedUp;
}

bool CpuSessionState::cancelRequested() const
{
    return m_cancelRequested;
}

void CpuSessionState::markWarmedUp()
{
    m_cancelRequested = false;
    m_warmedUp = true;
}

void CpuSessionState::requestCancel()
{
    m_cancelRequested = true;
    m_bufferedSamples.clear();
}

void CpuSessionState::resetForNextUtterance()
{
    m_cancelRequested = false;
    m_bufferedSamples.clear();
    m_kvCache.reset();
}

CpuKVCache &CpuSessionState::kvCache()
{
    return m_kvCache;
}

void CpuSessionState::allocateKVCache(const CpuModelConfig &config, int maxDecoderTokens)
{
    m_kvCache.allocate(config, maxDecoderTokens);
}
