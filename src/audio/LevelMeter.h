#pragma once

#include <atomic>
#include <cmath>

namespace kb
{
/** RMS- og peak-måler for to kanaler. Regnes ut på lydtråden og leses av UI via atomics. */
class LevelMeter
{
public:
    /** Kalles fra lydtråden, én gang per lydblokk. Ingen allokering eller låser. */
    void processBlock (const float* left, const float* right, int numSamples, double sampleRate) noexcept
    {
        if (numSamples <= 0 || sampleRate <= 0.0)
            return;

        const float* channels[2] = { left, right };
        const float peakDecay = static_cast<float> (std::exp (-static_cast<double> (numSamples) / (sampleRate * 0.35)));
        const float rmsAlpha = 1.0f - static_cast<float> (std::exp (-static_cast<double> (numSamples) / (sampleRate * 0.12)));

        for (int c = 0; c < 2; ++c)
        {
            float blockPeak = 0.0f;
            double sumSquares = 0.0;
            const float* data = channels[c];

            for (int i = 0; i < numSamples; ++i)
            {
                const float s = data[i];
                blockPeak = std::max (blockPeak, std::fabs (s));
                sumSquares += static_cast<double> (s) * static_cast<double> (s);
            }

            const float blockRms = static_cast<float> (std::sqrt (sumSquares / numSamples));
            const float prevPeak = peak[c].load (std::memory_order_relaxed);
            const float prevRms = rms[c].load (std::memory_order_relaxed);

            peak[c].store (std::max (blockPeak, prevPeak * peakDecay), std::memory_order_relaxed);
            rms[c].store (prevRms + rmsAlpha * (blockRms - prevRms), std::memory_order_relaxed);
        }
    }

    float getPeak (int channel) const noexcept { return peak[channel & 1].load (std::memory_order_relaxed); }
    float getRms (int channel) const noexcept { return rms[channel & 1].load (std::memory_order_relaxed); }

    /** Låses på når lyden gikk over 0 dBFS (før limiteren). Kan nullstilles med et klikk. */
    void setClipped() noexcept { clipped.store (true, std::memory_order_relaxed); }
    bool isClipped() const noexcept { return clipped.load (std::memory_order_relaxed); }
    void resetClip() noexcept { clipped.store (false, std::memory_order_relaxed); }

    void reset() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            peak[c].store (0.0f);
            rms[c].store (0.0f);
        }
        clipped.store (false);
    }

private:
    std::atomic<float> peak[2] { 0.0f, 0.0f };
    std::atomic<float> rms[2] { 0.0f, 0.0f };
    std::atomic<bool> clipped { false };
};
} // namespace kb
