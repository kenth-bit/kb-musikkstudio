#pragma once

#include <cmath>
#include <string>

namespace kb
{
/** MIDI-notenummer til frekvens i Hz. 69 = A4 = 440 Hz. 60 = midtre C = 261.63 Hz. */
inline double noteToHz (int note) noexcept
{
    return 440.0 * std::pow (2.0, (static_cast<double> (note) - 69.0) / 12.0);
}

/** Notenavn med oktav. 60 = "C4" (midtre C). */
inline std::string noteName (int note)
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int index = ((note % 12) + 12) % 12;
    const int octave = (note / 12) - 1;
    return std::string (names[index]) + std::to_string (octave);
}

/** Velocity 0..127 til volum 0..1. Myk kurve: svake anslag blir tydelig svakere. */
inline float velocityToGain (int velocity) noexcept
{
    if (velocity <= 0)
        return 0.0f;
    if (velocity >= 127)
        return 1.0f;

    return std::pow (static_cast<float> (velocity) / 127.0f, 1.3f);
}
} // namespace kb
