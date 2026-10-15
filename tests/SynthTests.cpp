#include "SynthRig.h"

#include <chrono>

using namespace kb;
using kbtest::SynthRig;

KB_TEST (Synth, one_note_makes_sound_and_one_voice)
{
    SynthRig rig;
    float peak = rig.block ({ rig.press (60, 100) }, 256);
    peak = std::max (peak, rig.run (0.2));
    KB_CHECK (peak > 0.01f);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 1);
    KB_CHECK (rig.synth.isNoteActive (60));
    KB_CHECK_EQ (rig.synth.getNumGatedVoices(), 1);
    KB_CHECK_EQ (rig.counters.noteOnProcessed.load(), 1u);
}

KB_TEST (Synth, eight_notes_use_eight_voices)
{
    SynthRig rig;
    std::vector<MidiEvent> events;
    for (int i = 0; i < 8; ++i)
        events.push_back (rig.press (48 + i * 2, 90));

    rig.block (events, 256);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 8);
    KB_CHECK_EQ (rig.synth.getNumGatedVoices(), 8);
    for (int i = 0; i < 8; ++i)
        KB_CHECK (rig.synth.isNoteActive (48 + i * 2));
}

KB_TEST (Synth, ninth_note_steals_a_voice_but_never_exceeds_eight)
{
    SynthRig rig;
    std::vector<MidiEvent> events;
    for (int i = 0; i < 9; ++i)
        events.push_back (rig.press (40 + i, 90));

    rig.block (events, 256);
    rig.run (0.05);

    KB_CHECK (rig.synth.getActiveVoices() <= 8);
    KB_CHECK (rig.synth.isNoteActive (48));
    KB_CHECK_EQ (rig.synth.getNumGatedVoices(), 8);
}

KB_TEST (Synth, stealing_prefers_released_voices_over_held_ones)
{
    SynthRig rig;
    std::vector<MidiEvent> events;
    for (int i = 0; i < 8; ++i)
        events.push_back (rig.press (40 + i, 90));
    rig.block (events, 256);
    rig.run (0.1);

    rig.block ({ rig.release (40), rig.release (41) }, 128);
    rig.block ({ rig.press (80, 90) }, 128);
    rig.run (0.05);

    for (int note = 42; note < 48; ++note)
        KB_CHECK_MSG (rig.synth.isNoteActive (note), "held note " + std::to_string (note) + " must still sound");
    KB_CHECK (rig.synth.isNoteActive (80));
}

KB_TEST (Synth, same_note_twice_reuses_the_same_voice)
{
    SynthRig rig;
    rig.block ({ rig.press (60, 100) }, 128);
    rig.block ({ rig.press (60, 80) }, 128);
    rig.block ({ rig.press (60, 60) }, 128);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 1);
    rig.block ({ rig.release (60) }, 128);
    KB_CHECK_EQ (rig.synth.getNumGatedVoices(), 0);
}

KB_TEST (Synth, note_off_releases_and_then_goes_silent)
{
    SynthRig rig;
    rig.setParam ("synth_release", 0.3f);
    rig.block ({ rig.press (60, 100) }, 256);
    rig.run (0.2);
    rig.block ({ rig.release (60) }, 256);
    KB_CHECK_EQ (rig.synth.getNumGatedVoices(), 0);
    KB_CHECK (rig.synth.getActiveVoices() == 1);

    rig.run (1.0);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 0);
    KB_CHECK_EQ (rig.block (256), 0.0f);
}

KB_TEST (Synth, sustain_pedal_holds_notes_until_released)
{
    SynthRig rig;
    rig.setParam ("synth_release", 0.2f);

    rig.block ({ rig.pedal (true), rig.press (60, 100), rig.press (64, 100) }, 256);
    rig.block ({ rig.release (60), rig.release (64) }, 256);
    KB_CHECK_EQ (rig.synth.getNumGatedVoices(), 0);
    KB_CHECK_EQ (rig.synth.getNumSustainedVoices(), 2);

    const float held = rig.run (2.0);
    KB_CHECK (held > 0.01f);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 2);

    rig.block ({ rig.pedal (false) }, 256);
    KB_CHECK_EQ (rig.synth.getNumSustainedVoices(), 0);
    rig.run (0.8);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 0);
}

KB_TEST (Synth, key_pressed_while_pedal_is_down_and_still_held_keeps_sounding_after_pedal_up)
{
    SynthRig rig;
    rig.block ({ rig.pedal (true), rig.press (60, 100) }, 256);
    rig.block ({ rig.pedal (false) }, 256);
    rig.run (0.3);
    KB_CHECK (rig.synth.isNoteActive (60));
    KB_CHECK_EQ (rig.synth.getNumGatedVoices(), 1);
}

KB_TEST (Synth, panic_silences_everything_within_ten_ms)
{
    SynthRig rig;
    std::vector<MidiEvent> events;
    for (int i = 0; i < 8; ++i)
        events.push_back (rig.press (48 + i, 127));
    events.push_back (rig.pedal (true));
    rig.block (events, 256);
    rig.run (0.2);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 8);

    rig.synth.allNotesOff();
    rig.state.releaseAll();
    rig.run (0.012, 64);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 0);
    KB_CHECK_EQ (rig.block (256), 0.0f);
}

KB_TEST (Synth, all_notes_off_event_also_clears_the_pedal)
{
    SynthRig rig (48000.0, 1024, false);
    rig.block ({ MidiEvent::sustain (true), MidiEvent::noteOn (60, 100) }, 256);
    rig.block ({ MidiEvent::allNotesOff() }, 256);
    rig.run (0.05);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 0);

    rig.block ({ MidiEvent::noteOn (62, 100), MidiEvent::noteOff (62) }, 256);
    rig.run (3.0);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 0);
}

KB_TEST (Synth, velocity_controls_loudness)
{
    float loud, soft;
    {
        SynthRig rig;
        rig.setParam ("synth_attack", 0.001f);
        loud = rig.block ({ rig.press (60, 127) }, 512) + 0.0f;
        loud = std::max (loud, rig.run (0.1));
    }
    {
        SynthRig rig;
        rig.setParam ("synth_attack", 0.001f);
        soft = rig.block ({ rig.press (60, 30) }, 512);
        soft = std::max (soft, rig.run (0.1));
    }
    KB_CHECK (loud > soft * 2.0f);
    KB_CHECK (soft > 0.001f);
}

KB_TEST (Synth, output_is_finite_and_bounded_in_the_worst_case)
{
    SynthRig rig;
    rig.setParam ("synth_volume", 1.0f);
    rig.setParam ("synth_detune", 50.0f);
    rig.setParam ("synth_attack", 0.001f);
    rig.setParam ("synth_sustain", 1.0f);

    std::vector<MidiEvent> events;
    for (int i = 0; i < 12; ++i)
        events.push_back (rig.press (21 + i * 7, 127));

    rig.block (events, 256);
    rig.run (1.0);

    KB_CHECK_EQ (rig.nonFiniteBlocks, 0);
    KB_CHECK (rig.maxPeak < 1.6f);
}

KB_TEST (Synth, extreme_parameter_values_never_produce_nan)
{
    kbtest::Rng rng (99);
    SynthRig rig;
    const char* ids[] = { "synth_volume", "synth_detune", "synth_attack", "synth_decay", "synth_sustain", "synth_release" };

    for (int round = 0; round < 300; ++round)
    {
        for (const auto* id : ids)
        {
            auto& p = rig.registry.require (id);
            p.set (static_cast<float> (p.info().minValue + rng.unit() * (p.info().maxValue - p.info().minValue)));
            if (rng.chance (0.2))
                p.set (rng.chance (0.5) ? p.info().minValue : p.info().maxValue);
        }

        std::vector<MidiEvent> events;
        for (int i = 0; i < rng.range (0, 4); ++i)
            events.push_back (rng.chance (0.6) ? rig.press (rng.range (21, 108), rng.range (1, 127)) : rig.release (rng.range (21, 108)));

        rig.block (events, rng.range (1, 512));
    }

    KB_CHECK_EQ (rig.nonFiniteBlocks, 0);
    KB_CHECK (rig.maxPeak < 3.0f);
}

KB_TEST (Synth, parameters_ignore_nan_and_clamp)
{
    SynthRig rig;
    auto& attack = rig.registry.require ("synth_attack");
    attack.set (0.5f);
    attack.set (std::nanf (""));
    KB_CHECK_EQ (attack.get(), 0.5f);
    attack.set (1.0e9f);
    KB_CHECK_EQ (attack.get(), attack.info().maxValue);
    attack.set (-5.0f);
    KB_CHECK_EQ (attack.get(), attack.info().minValue);
}

KB_TEST (Synth, lost_note_off_is_caught_by_the_watchdog)
{
    SynthRig rig;
    rig.setParam ("synth_release", 0.2f);
    rig.block ({ rig.press (60, 100), rig.press (64, 100), rig.press (67, 100) }, 256);
    rig.run (0.2);

    rig.release (60);
    rig.release (64);
    rig.release (67);
    rig.block (256);

    KB_CHECK_EQ (rig.synth.getNumGatedVoices(), 0);
    KB_CHECK_EQ (rig.counters.watchdogReleases.load(), 3u);
    rig.run (1.0);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 0);
}

KB_TEST (Synth, lost_pedal_up_event_is_caught_by_the_watchdog)
{
    SynthRig rig;
    rig.setParam ("synth_release", 0.2f);
    rig.block ({ rig.pedal (true), rig.press (60, 100) }, 256);
    rig.block ({ rig.release (60) }, 256);
    KB_CHECK_EQ (rig.synth.getNumSustainedVoices(), 1);

    rig.pedal (false);
    rig.block (256);
    KB_CHECK_EQ (rig.synth.getNumSustainedVoices(), 0);
    rig.run (1.0);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 0);
}

KB_TEST (Synth, release_cap_ends_a_voice_even_with_a_very_long_release)
{
    SynthRig rig;
    rig.setParam ("synth_release", 8.0f);
    rig.block ({ rig.press (60, 100) }, 256);
    rig.run (0.1);
    rig.block ({ rig.release (60) }, 256);

    rig.run (2.0 * 8.0 + 0.5);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 0);
}

KB_TEST (Synth, steal_cancelled_by_note_off_in_the_same_block_leaves_nothing_hanging)
{
    SynthRig rig;
    std::vector<MidiEvent> held;
    for (int i = 0; i < 8; ++i)
        held.push_back (rig.press (40 + i, 90));
    rig.block (held, 256);

    rig.block ({ rig.press (90, 100), rig.release (90) }, 256);
    rig.run (0.05);
    KB_CHECK (! rig.synth.isNoteActive (90));
    KB_CHECK_EQ (rig.synth.getNumGatedVoices(), 7);

    std::vector<MidiEvent> offs;
    for (int i = 0; i < 8; ++i)
        offs.push_back (rig.release (40 + i));
    rig.block (offs, 256);
    rig.setParam ("synth_release", 0.1f);
    rig.run (2.0);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 0);
}

KB_TEST (Synth, twelve_simultaneous_notes_in_the_same_sample_then_released_together)
{
    SynthRig rig;
    rig.setParam ("synth_release", 0.2f);

    std::vector<MidiEvent> events;
    for (int i = 0; i < 12; ++i)
        events.push_back (rig.press (50 + i, 100, 0));
    rig.block (events, 256);
    rig.run (0.1);
    KB_CHECK (rig.synth.getActiveVoices() <= 8);

    events.clear();
    for (int i = 0; i < 12; ++i)
        events.push_back (rig.release (50 + i, 0));
    rig.block (events, 256);
    rig.run (1.0);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 0);
}

KB_TEST (Synth, events_inside_a_block_are_applied_at_their_sample_position)
{
    SynthRig rig;
    rig.setParam ("synth_attack", 0.001f);

    const float peak = rig.block ({ rig.press (60, 127, 400) }, 512);
    KB_CHECK (peak > 0.0f);

    float leadingPeak = 0.0f;
    for (int i = 0; i < 400; ++i)
        leadingPeak = std::max (leadingPeak, std::fabs (rig.left[static_cast<size_t> (i)]));
    KB_CHECK_EQ (leadingPeak, 0.0f);
}

KB_TEST (Synth, prepare_clears_all_voices_and_state)
{
    SynthRig rig;
    rig.block ({ rig.press (60, 100), rig.pedal (true) }, 256);
    rig.synth.prepare (44100.0, 512);
    rig.state.reset();
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 0);
    KB_CHECK_EQ (rig.block (256), 0.0f);
}

KB_TEST (Synth, presets_set_the_parameters)
{
    SynthRig rig;
    const auto& presets = SynthModule::presets();
    KB_CHECK_EQ (presets.size(), static_cast<size_t> (4));

    for (size_t i = 0; i < presets.size(); ++i)
    {
        rig.synth.applyPreset (static_cast<int> (i));
        KB_CHECK_NEAR (rig.registry.require ("synth_attack").get(), presets[i].attack, 1.0e-6);
        KB_CHECK_NEAR (rig.registry.require ("synth_release").get(), presets[i].release, 1.0e-6);
        KB_CHECK_NEAR (rig.registry.require ("synth_detune").get(), presets[i].detune, 1.0e-6);
        KB_CHECK (! rig.registry.getHelp (presets[i].id).isEmpty());
    }

    rig.synth.applyPreset (-1);
    rig.synth.applyPreset (99);
}

KB_TEST (Synth, works_at_44100_and_with_tiny_and_huge_blocks)
{
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
    {
        SynthRig rig (rate, 4096);
        rig.block ({ rig.press (60, 100), rig.press (64, 100) }, 1);
        rig.block (4096);
        rig.block (1);
        rig.block ({ rig.release (60), rig.release (64) }, 3);
        rig.setParam ("synth_release", 0.1f);
        rig.run (1.5, 777);
        KB_CHECK_EQ (rig.synth.getActiveVoices(), 0);
        KB_CHECK_EQ (rig.nonFiniteBlocks, 0);
    }
}

KB_TEST (Synth, performance_eight_voices_render_faster_than_real_time)
{
    SynthRig rig (48000.0, 256);
    rig.setParam ("synth_sustain", 1.0f);

    std::vector<MidiEvent> events;
    for (int i = 0; i < 8; ++i)
        events.push_back (rig.press (40 + i * 5, 100));
    rig.block (events, 256);

    const double seconds = 5.0;
    const int blocks = static_cast<int> (seconds * 48000.0 / 256.0);

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < blocks; ++i)
        rig.block (256);
    const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();

#ifdef NDEBUG
    const double limit = 0.10;
#else
    const double limit = 0.60;
#endif
    std::printf ("    %.1f s lyd med 8 stemmer ble regnet ut på %.3f s (%.1f %% av sanntid)\n", seconds, elapsed,
                 100.0 * elapsed / seconds);
    KB_CHECK (elapsed < seconds * limit);
}
