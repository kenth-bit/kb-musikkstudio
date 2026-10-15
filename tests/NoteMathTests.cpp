#include "TestFramework.h"

#include "midi/NoteMath.h"

using namespace kb;

KB_TEST (NoteMath, a4_is_440_hz)
{
    KB_CHECK_NEAR (noteToHz (69), 440.0, 1.0e-9);
}

KB_TEST (NoteMath, middle_c_is_261_63_hz)
{
    KB_CHECK_NEAR (noteToHz (60), 261.6256, 0.001);
}

KB_TEST (NoteMath, piano_range_end_points)
{
    KB_CHECK_NEAR (noteToHz (21), 27.5, 1.0e-6);
    KB_CHECK_NEAR (noteToHz (108), 4186.009, 0.001);
}

KB_TEST (NoteMath, octave_doubles_frequency)
{
    for (int note = 0; note < 116; ++note)
        KB_CHECK_NEAR (noteToHz (note + 12) / noteToHz (note), 2.0, 1.0e-9);
}

KB_TEST (NoteMath, note_names)
{
    KB_CHECK_EQ (noteName (60), std::string ("C4"));
    KB_CHECK_EQ (noteName (61), std::string ("C#4"));
    KB_CHECK_EQ (noteName (69), std::string ("A4"));
    KB_CHECK_EQ (noteName (21), std::string ("A0"));
    KB_CHECK_EQ (noteName (108), std::string ("C8"));
    KB_CHECK_EQ (noteName (0), std::string ("C-1"));
    KB_CHECK_EQ (noteName (127), std::string ("G9"));
}

KB_TEST (NoteMath, velocity_curve_is_monotonic_and_bounded)
{
    KB_CHECK_EQ (velocityToGain (0), 0.0f);
    KB_CHECK_EQ (velocityToGain (127), 1.0f);

    float previous = 0.0f;
    for (int v = 1; v <= 127; ++v)
    {
        const float gain = velocityToGain (v);
        KB_CHECK (std::isfinite (gain));
        KB_CHECK (gain > previous);
        KB_CHECK (gain <= 1.0f);
        previous = gain;
    }
}

KB_TEST (NoteMath, velocity_out_of_range_is_safe)
{
    KB_CHECK_EQ (velocityToGain (-5), 0.0f);
    KB_CHECK_EQ (velocityToGain (500), 1.0f);
}

KB_TEST (NoteMath, soft_velocity_is_clearly_softer)
{
    KB_CHECK (velocityToGain (32) < 0.25f);
    KB_CHECK (velocityToGain (96) > 0.6f);
}
