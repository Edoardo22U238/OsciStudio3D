#include "AudioEngine.h"
#include <algorithm>
#include <cstring>

AudioEngine::AudioEngine()
{
    std::memset (bufs_[0].L, 0, sizeof (bufs_[0].L));
    std::memset (bufs_[0].R, 0, sizeof (bufs_[0].R));
    std::memset (bufs_[1].L, 0, sizeof (bufs_[1].L));
    std::memset (bufs_[1].R, 0, sizeof (bufs_[1].R));
}

void AudioEngine::setPath (const std::vector<float>& x,
                            const std::vector<float>& y)
{
    if (x.empty() || y.empty()) return;

    const int n   = static_cast<int> (std::min ({ x.size(), y.size(),
                                                  static_cast<size_t>(MAX_SAMPLES) }));
    const int nxt = 1 - active_.load (std::memory_order_relaxed);

    std::memcpy (bufs_[nxt].L, x.data(), (size_t)n * sizeof (float));
    std::memcpy (bufs_[nxt].R, y.data(), (size_t)n * sizeof (float));
    bufs_[nxt].length = n;

    pendingSwap_.store (true, std::memory_order_release);
}

void AudioEngine::fill (const juce::AudioSourceChannelInfo& info) noexcept
{
    // Swap buffer if geometry thread prepared a new one
    if (pendingSwap_.load (std::memory_order_acquire))
    {
        active_.store (1 - active_.load (std::memory_order_relaxed),
                       std::memory_order_relaxed);
        pendingSwap_.store (false, std::memory_order_relaxed);
        pos_ = 0;
    }

    const StereoBuffer& buf = bufs_[active_.load (std::memory_order_relaxed)];
    const int n = buf.length;

    auto* outL = info.buffer->getWritePointer (0, info.startSample);
    auto* outR = info.buffer->getWritePointer (1, info.startSample);

    if (!playing_.load (std::memory_order_relaxed) || n == 0)
    {
        std::memset (outL, 0, (size_t)info.numSamples * sizeof (float));
        std::memset (outR, 0, (size_t)info.numSamples * sizeof (float));
        return;
    }

    const float gx = gainX.load  (std::memory_order_relaxed);
    const float gy = gainY.load  (std::memory_order_relaxed);
    const float gm = master.load (std::memory_order_relaxed);
    const int   ps = phaseShift.load (std::memory_order_relaxed);

    for (int i = 0; i < info.numSamples; ++i)
    {
        const int idxL = pos_ % n;
        const int idxR = (pos_ + ps) % n;
        outL[i] = buf.L[idxL] * gx * gm;
        outR[i] = buf.R[idxR] * gy * gm;
        // clamp in-place
        outL[i] = outL[i] < -1.0f ? -1.0f : (outL[i] > 1.0f ? 1.0f : outL[i]);
        outR[i] = outR[i] < -1.0f ? -1.0f : (outR[i] > 1.0f ? 1.0f : outR[i]);
        if (++pos_ >= n) pos_ = 0;
    }
}

float AudioEngine::currentHz() const noexcept
{
    const int n = bufs_[active_.load (std::memory_order_relaxed)].length;
    return n > 0 ? AudioEngine::DEFAULT_SR / static_cast<float> (n) : 0.0f;
}

int AudioEngine::currentSamples() const noexcept
{
    return bufs_[active_.load (std::memory_order_relaxed)].length;
}
