#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <cstddef>

/**
 * Lock-free double-buffered audio engine.
 * Audio callback ONLY reads pre-built buffers — zero allocations.
 * Geometry thread writes to the inactive buffer and sets a swap flag.
 */
class AudioEngine
{
public:
    static constexpr int   MAX_SAMPLES = 1 << 17;   // 131 072  (~2.7 s @ 48 kHz)
    static constexpr float DEFAULT_SR  = 48000.0f;

    AudioEngine();

    // ── Called from geometry/UI thread ──────────────────────
    /** Push a new XY path. Thread-safe, non-blocking. */
    void setPath (const std::vector<float>& x,
                  const std::vector<float>& y);

    // ── Called from audio thread ─────────────────────────────
    /** Fill output buffer. Zero allocations, zero locks. */
    void fill (const juce::AudioSourceChannelInfo& info) noexcept;

    // ── Parameters (written from UI thread) ─────────────────
    std::atomic<float> gainX    { 1.0f };
    std::atomic<float> gainY    { 1.0f };
    std::atomic<float> master   { 1.0f };
    std::atomic<int>   phaseShift { 0 };  // samples Y shifts ahead of X
    std::atomic<bool>  playing  { false };

    // ── Diagnostics ─────────────────────────────────────────
    float currentHz()      const noexcept;
    int   currentSamples() const noexcept;

private:
    struct StereoBuffer
    {
        float L[MAX_SAMPLES];
        float R[MAX_SAMPLES];
        int   length { 0 };
    };

    StereoBuffer          bufs_[2];
    std::atomic<int>      active_      { 0 };
    std::atomic<bool>     pendingSwap_ { false };
    int                   pos_         { 0 };   // audio thread only
};
