#pragma once

#include <algorithm>
#include <cmath>

namespace kb
{
inline float dbToGain (float db) noexcept
{
    return db <= -100.0f ? 0.0f : std::pow (10.0f, db / 20.0f);
}

inline float gainToDb (float gain) noexcept
{
    return gain <= 0.00001f ? -100.0f : 20.0f * std::log10 (gain);
}

/** Myk limiter: slipper aldri gjennom noe over 0 dBFS, og stopper NaN og uendelig.
    Under 0.85 går lyden uendret gjennom. Over det rundes toppene mykt av mot 1.0. */
struct SoftLimiter
{
    static constexpr float knee = 0.85f;

    static float process (float x) noexcept
    {
        if (! std::isfinite (x))
            return 0.0f;

        const float ax = std::fabs (x);
        if (ax <= knee)
            return x;

        const float room = 1.0f - knee;
        const float y = std::min (knee + room * std::tanh ((ax - knee) / room), 1.0f);
        return std::copysign (y, x);
    }
};
} // namespace kb
