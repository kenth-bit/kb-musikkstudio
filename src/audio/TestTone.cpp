#include "audio/TestTone.h"

#include "audio/Dsp.h"

#include <algorithm>
#include <cmath>

namespace kb
{
void TestTone::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    fadeStep = static_cast<float> (1.0 / (static_cast<double> (fadeSeconds) * sampleRate));
    level = dbToGain (levelDb);
    phase = 0.0;
    fade = 0.0f;
}

void TestTone::addTo (float* left, float* right, int numSamples) noexcept
{
    const bool wanted = isEnabled();

    if (! wanted && fade <= 0.0f)
        return;

    const double increment = static_cast<double> (frequencyHz) / sampleRate;
    constexpr double twoPi = 6.283185307179586;

    for (int i = 0; i < numSamples; ++i)
    {
        fade = wanted ? std::min (1.0f, fade + fadeStep) : std::max (0.0f, fade - fadeStep);

        const float sample = static_cast<float> (std::sin (phase * twoPi)) * level * fade;
        left[i] += sample;
        right[i] += sample;

        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;
    }
}
} // namespace kb
