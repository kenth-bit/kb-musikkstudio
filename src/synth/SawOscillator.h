#pragma once

#include <algorithm>

namespace kb
{
/** Sagtannbølge med PolyBLEP, så den ikke aliaserer (ikke gir skarpe "falske" toner i diskanten). */
class SawOscillator
{
public:
    void setFrequency (double hz, double sampleRate) noexcept
    {
        increment = std::clamp (hz / sampleRate, 0.0, 0.45);
    }

    void setPhase (double newPhase) noexcept
    {
        phase = newPhase - static_cast<long long> (newPhase);
        if (phase < 0.0)
            phase += 1.0;
    }

    float next() noexcept
    {
        const double t = phase;
        float value = static_cast<float> (2.0 * t - 1.0);
        value -= static_cast<float> (polyBlep (t));

        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;

        return value;
    }

private:
    double polyBlep (double t) const noexcept
    {
        const double dt = increment;
        if (dt <= 0.0)
            return 0.0;

        if (t < dt)
        {
            const double x = t / dt;
            return x + x - x * x - 1.0;
        }

        if (t > 1.0 - dt)
        {
            const double x = (t - 1.0) / dt;
            return x * x + x + x + 1.0;
        }

        return 0.0;
    }

    double phase = 0.0;
    double increment = 0.0;
};
} // namespace kb
