#include "TestFramework.h"
#include "TestUtil.h"

#include "audio/Dsp.h"
#include "audio/LevelMeter.h"
#include "audio/TestTone.h"
#include "synth/Envelope.h"
#include "synth/SawOscillator.h"

#include <cmath>
#include <limits>
#include <vector>

using namespace kb;

namespace
{
constexpr double sr = 48000.0;

int samplesUntil (Envelope& env, int maxSamples, bool (*predicate) (const Envelope&))
{
    for (int i = 0; i < maxSamples; ++i)
    {
        env.next();
        if (predicate (env))
            return i + 1;
    }
    return -1;
}
} // namespace

// --- Envelope ----------------------------------------------------------------------------

KB_TEST (Envelope, attack_takes_the_set_time_and_reaches_full_level)
{
    Envelope env;
    env.setCoefficients (EnvelopeCoefficients::make (sr, 0.1f, 0.5f, 0.6f, 0.5f));
    env.noteOn();

    const int n = samplesUntil (env, 20000, [] (const Envelope& e) { return e.getStage() != Envelope::Stage::Attack; });
    KB_CHECK_NEAR (n, 0.1 * sr, 5.0);
    KB_CHECK_NEAR (env.getLevel(), 1.0, 1.0e-6);
}

KB_TEST (Envelope, decay_settles_on_sustain_level)
{
    Envelope env;
    env.setCoefficients (EnvelopeCoefficients::make (sr, 0.002f, 0.2f, 0.6f, 0.5f));
    env.noteOn();

    for (int i = 0; i < static_cast<int> (3.0 * sr); ++i)
        env.next();

    KB_CHECK (env.getStage() == Envelope::Stage::Sustain);
    KB_CHECK_NEAR (env.getLevel(), 0.6, 1.0e-3);
}

KB_TEST (Envelope, release_ends_in_idle_within_bounded_time)
{
    for (const float release : { 0.005f, 0.1f, 1.0f, 4.0f })
    {
        Envelope env;
        env.setCoefficients (EnvelopeCoefficients::make (sr, 0.001f, 0.1f, 0.8f, release));
        env.noteOn();
        for (int i = 0; i < 20000; ++i)
            env.next();

        env.noteOff();
        const int n = samplesUntil (env, static_cast<int> (release * 1.2 * sr) + 100, [] (const Envelope& e) { return e.isIdle(); });
        KB_CHECK_MSG (n > 0, "release " + std::to_string (release));
    }
}

KB_TEST (Envelope, note_off_in_attack_has_no_jump_in_level)
{
    Envelope env;
    env.setCoefficients (EnvelopeCoefficients::make (sr, 0.5f, 0.1f, 0.8f, 0.5f));
    env.noteOn();
    for (int i = 0; i < 6000; ++i)
        env.next();

    const float before = env.getLevel();
    env.noteOff();
    const float after = env.next();
    KB_CHECK (before > 0.1f && before < 0.9f);
    KB_CHECK (after <= before);
    KB_CHECK_NEAR (after, before, 0.01);
    KB_CHECK (env.getStage() == Envelope::Stage::Release);
}

KB_TEST (Envelope, retrigger_continues_from_current_level_without_click)
{
    Envelope env;
    env.setCoefficients (EnvelopeCoefficients::make (sr, 0.05f, 0.1f, 0.8f, 1.0f));
    env.noteOn();
    for (int i = 0; i < 20000; ++i)
        env.next();
    env.noteOff();
    for (int i = 0; i < 4000; ++i)
        env.next();

    const float before = env.getLevel();
    env.noteOn();
    const float after = env.next();
    KB_CHECK (before > 0.05f);
    KB_CHECK_NEAR (after, before, 0.01);
}

KB_TEST (Envelope, fast_release_reaches_silence_in_about_five_ms)
{
    Envelope env;
    env.setCoefficients (EnvelopeCoefficients::make (sr, 0.001f, 0.1f, 1.0f, 5.0f));
    env.noteOn();
    for (int i = 0; i < 5000; ++i)
        env.next();

    env.fastRelease();
    const int n = samplesUntil (env, 4800, [] (const Envelope& e) { return e.isIdle(); });
    KB_CHECK (n > 0);
    KB_CHECK (n < static_cast<int> (0.012 * sr));
}

KB_TEST (Envelope, zero_and_extreme_times_are_safe)
{
    const float values[] = { 0.0f, -1.0f, 1.0e-9f, 100.0f, std::numeric_limits<float>::infinity() };
    for (const float a : values)
    {
        for (const float r : values)
        {
            Envelope env;
            env.setCoefficients (EnvelopeCoefficients::make (sr, a, 0.0f, 2.0f, r));
            env.noteOn();
            for (int i = 0; i < 3000; ++i)
            {
                const float level = env.next();
                KB_CHECK (std::isfinite (level));
                KB_CHECK (level >= 0.0f && level <= 1.0001f);
            }
            env.noteOff();
            for (int i = 0; i < 3000; ++i)
                KB_CHECK (std::isfinite (env.next()));
        }
    }
}

KB_TEST (Envelope, idle_envelope_stays_silent)
{
    Envelope env;
    env.noteOff();
    env.fastRelease();
    KB_CHECK (env.isIdle());
    KB_CHECK_EQ (env.next(), 0.0f);
}

// --- Oscillator --------------------------------------------------------------------------

KB_TEST (SawOscillator, has_the_right_frequency)
{
    for (const double hz : { 110.0, 261.6256, 1000.0, 4186.0 })
    {
        SawOscillator osc;
        osc.setFrequency (hz, sr);

        int risingZeroCrossings = 0;
        float previous = osc.next();
        const int count = static_cast<int> (sr);
        for (int i = 1; i < count; ++i)
        {
            const float s = osc.next();
            if (previous < 0.0f && s >= 0.0f)
                ++risingZeroCrossings;
            previous = s;
        }
        KB_CHECK_NEAR (risingZeroCrossings, hz, 1.5);
    }
}

KB_TEST (SawOscillator, stays_bounded_and_has_no_dc)
{
    SawOscillator osc;
    osc.setFrequency (440.0, sr);

    double sum = 0.0;
    float peak = 0.0f;
    const int count = static_cast<int> (sr);
    for (int i = 0; i < count; ++i)
    {
        const float s = osc.next();
        KB_CHECK (std::isfinite (s));
        peak = std::max (peak, std::fabs (s));
        sum += s;
    }

    KB_CHECK (peak <= 1.1f);
    KB_CHECK (peak > 0.9f);
    KB_CHECK_NEAR (sum / count, 0.0, 0.01);
}

KB_TEST (SawOscillator, extreme_frequencies_are_safe)
{
    for (const double hz : { 0.0, -50.0, 1.0e9, 24000.0, 1.0e-6 })
    {
        SawOscillator osc;
        osc.setFrequency (hz, sr);
        osc.setPhase (7.3);
        for (int i = 0; i < 10000; ++i)
        {
            const float s = osc.next();
            KB_CHECK (std::isfinite (s));
            KB_CHECK (std::fabs (s) <= 1.5f);
        }
    }
}

KB_TEST (SawOscillator, polyblep_reduces_aliasing_compared_to_naive_saw)
{
    // Høy tone: den naive sagtannen har mye energi i "falske" lave frekvenser. Mål forskjellen
    // mellom to påfølgende prøver: PolyBLEP gjør hoppet ved bølgeslutten mykere.
    SawOscillator osc;
    osc.setFrequency (3000.0, sr);

    double naiveMaxJump = 0.0, blepMaxJump = 0.0;
    double phase = 0.0;
    float prevBlep = osc.next();
    double prevNaive = 2.0 * phase - 1.0;

    for (int i = 0; i < 20000; ++i)
    {
        phase += 3000.0 / sr;
        if (phase >= 1.0) phase -= 1.0;
        const double naive = 2.0 * phase - 1.0;
        const float blep = osc.next();
        naiveMaxJump = std::max (naiveMaxJump, std::fabs (naive - prevNaive));
        blepMaxJump = std::max (blepMaxJump, static_cast<double> (std::fabs (blep - prevBlep)));
        prevNaive = naive;
        prevBlep = blep;
    }

    KB_CHECK (blepMaxJump < naiveMaxJump);
}

// --- Limiter -----------------------------------------------------------------------------

KB_TEST (SoftLimiter, never_exceeds_one_and_handles_nan_and_infinity)
{
    const float inf = std::numeric_limits<float>::infinity();
    KB_CHECK_EQ (SoftLimiter::process (std::numeric_limits<float>::quiet_NaN()), 0.0f);
    KB_CHECK_EQ (SoftLimiter::process (inf), 0.0f);
    KB_CHECK_EQ (SoftLimiter::process (-inf), 0.0f);

    for (float x = -1000.0f; x <= 1000.0f; x += 0.37f)
    {
        const float y = SoftLimiter::process (x);
        KB_CHECK (std::isfinite (y));
        KB_CHECK (std::fabs (y) <= 1.0f);
    }
}

KB_TEST (SoftLimiter, passes_quiet_signals_unchanged)
{
    for (float x = -0.85f; x <= 0.85f; x += 0.01f)
        KB_CHECK_EQ (SoftLimiter::process (x), x);
}

KB_TEST (SoftLimiter, is_monotonic_continuous_and_odd)
{
    float previous = SoftLimiter::process (0.0f);
    for (float x = 0.001f; x < 20.0f; x += 0.001f)
    {
        const float y = SoftLimiter::process (x);
        KB_CHECK (y >= previous);
        KB_CHECK (y - previous < 0.01f);
        KB_CHECK_EQ (SoftLimiter::process (-x), -y);
        previous = y;
    }
}

KB_TEST (Dsp, db_conversions)
{
    KB_CHECK_NEAR (dbToGain (0.0f), 1.0, 1.0e-6);
    KB_CHECK_NEAR (dbToGain (-6.0206f), 0.5, 1.0e-4);
    KB_CHECK_NEAR (dbToGain (-12.0f), 0.2512, 1.0e-3);
    KB_CHECK_EQ (dbToGain (-100.0f), 0.0f);
    KB_CHECK_NEAR (gainToDb (1.0f), 0.0, 1.0e-6);
    KB_CHECK_NEAR (gainToDb (0.5f), -6.0206, 1.0e-3);
    KB_CHECK_EQ (gainToDb (0.0f), -100.0f);
}

// --- Måler -------------------------------------------------------------------------------

KB_TEST (LevelMeter, measures_peak_and_rms_of_a_sine)
{
    LevelMeter meter;
    std::vector<float> left (512), right (512);
    double phase = 0.0;

    for (int block = 0; block < 200; ++block)
    {
        for (int i = 0; i < 512; ++i)
        {
            left[static_cast<size_t> (i)] = 0.5f * static_cast<float> (std::sin (phase));
            right[static_cast<size_t> (i)] = 0.25f * static_cast<float> (std::sin (phase));
            phase += 0.0587;
        }
        meter.processBlock (left.data(), right.data(), 512, sr);
    }

    KB_CHECK_NEAR (meter.getPeak (0), 0.5, 0.01);
    KB_CHECK_NEAR (meter.getPeak (1), 0.25, 0.01);
    KB_CHECK_NEAR (meter.getRms (0), 0.5 / std::sqrt (2.0), 0.02);
    KB_CHECK_NEAR (meter.getRms (1), 0.25 / std::sqrt (2.0), 0.01);
}

KB_TEST (LevelMeter, peak_decays_after_silence)
{
    LevelMeter meter;
    std::vector<float> loud (512, 0.9f), quiet (512, 0.0f);
    meter.processBlock (loud.data(), loud.data(), 512, sr);
    KB_CHECK (meter.getPeak (0) >= 0.9f);

    for (int i = 0; i < 400; ++i)
        meter.processBlock (quiet.data(), quiet.data(), 512, sr);

    KB_CHECK (meter.getPeak (0) < 0.001f);
    KB_CHECK (meter.getRms (0) < 0.001f);
}

KB_TEST (LevelMeter, clip_latches_until_reset)
{
    LevelMeter meter;
    KB_CHECK (! meter.isClipped());
    meter.setClipped();
    std::vector<float> quiet (256, 0.0f);
    meter.processBlock (quiet.data(), quiet.data(), 256, sr);
    KB_CHECK (meter.isClipped());
    meter.resetClip();
    KB_CHECK (! meter.isClipped());
}

KB_TEST (LevelMeter, empty_and_invalid_blocks_are_safe)
{
    LevelMeter meter;
    float x = 0.0f;
    meter.processBlock (&x, &x, 0, sr);
    meter.processBlock (&x, &x, 1, 0.0);
    KB_CHECK (std::isfinite (meter.getPeak (0)));
    KB_CHECK (std::isfinite (meter.getRms (1)));
}

// --- Testtone ----------------------------------------------------------------------------

KB_TEST (TestTone, plays_440_hz_at_minus_18_db_with_fade)
{
    TestTone tone;
    tone.prepare (sr);
    tone.setEnabled (true);

    std::vector<float> left (static_cast<size_t> (sr), 0.0f), right (left);
    tone.addTo (left.data(), right.data(), static_cast<int> (sr));

    KB_CHECK_NEAR (left[0], 0.0, 1.0e-6);
    KB_CHECK_NEAR (kbtest::peakOf (left, static_cast<int> (sr)), dbToGain (-18.0f), 0.002);

    int crossings = 0;
    for (size_t i = 1; i < left.size(); ++i)
        if (left[i - 1] < 0.0f && left[i] >= 0.0f)
            ++crossings;
    KB_CHECK_NEAR (crossings, 440, 2);

    float maxStep = 0.0f;
    for (size_t i = 1; i < left.size(); ++i)
        maxStep = std::max (maxStep, std::fabs (left[i] - left[i - 1]));
    KB_CHECK (maxStep < 0.03f);
}

KB_TEST (TestTone, fades_out_without_a_click_and_then_adds_nothing)
{
    TestTone tone;
    tone.prepare (sr);
    tone.setEnabled (true);

    std::vector<float> left (4800, 0.0f), right (left);
    tone.addTo (left.data(), right.data(), 4800);
    tone.setEnabled (false);

    std::fill (left.begin(), left.end(), 0.0f);
    std::fill (right.begin(), right.end(), 0.0f);
    tone.addTo (left.data(), right.data(), 4800);

    float maxStep = 0.0f;
    for (size_t i = 1; i < left.size(); ++i)
        maxStep = std::max (maxStep, std::fabs (left[i] - left[i - 1]));
    KB_CHECK (maxStep < 0.03f);

    std::fill (left.begin(), left.end(), 0.0f);
    std::fill (right.begin(), right.end(), 0.0f);
    tone.addTo (left.data(), right.data(), 4800);
    KB_CHECK_EQ (kbtest::peakOf (left, 4800), 0.0f);
    KB_CHECK_EQ (kbtest::peakOf (right, 4800), 0.0f);
}

KB_TEST (TestTone, adds_to_existing_signal_in_both_channels)
{
    TestTone tone;
    tone.prepare (sr);
    tone.setEnabled (true);

    std::vector<float> left (2000, 0.25f), right (2000, -0.25f);
    tone.addTo (left.data(), right.data(), 2000);

    float diffL = 0.0f, diffR = 0.0f;
    for (size_t i = 0; i < 2000; ++i)
    {
        diffL = std::max (diffL, std::fabs (left[i] - 0.25f));
        diffR = std::max (diffR, std::fabs (right[i] + 0.25f));
    }
    KB_CHECK (diffL > 0.05f);
    KB_CHECK_NEAR (diffL, diffR, 1.0e-6);
}
