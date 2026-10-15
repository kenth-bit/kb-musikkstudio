#pragma once

#include <atomic>

namespace kb
{
/** Sinustone 440 Hz på lavt nivå (-18 dBFS) med myk inn- og utfading, slik at det ikke klikker. */
class TestTone
{
public:
    void prepare (double sampleRate) noexcept;

    void setEnabled (bool shouldPlay) noexcept { enabled.store (shouldPlay, std::memory_order_relaxed); }
    bool isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

    /** Legger tonen oppå signalet. Kalles fra lydtråden. */
    void addTo (float* left, float* right, int numSamples) noexcept;

    static constexpr float frequencyHz = 440.0f;
    static constexpr float levelDb = -18.0f;
    static constexpr float fadeSeconds = 0.015f;

private:
    std::atomic<bool> enabled { false };
    double sampleRate = 48000.0;
    double phase = 0.0;
    float fade = 0.0f;
    float fadeStep = 0.0f;
    float level = 0.125f;
};
} // namespace kb
