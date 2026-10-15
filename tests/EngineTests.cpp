#include "TestFramework.h"
#include "TestUtil.h"

#include "audio/AudioEngine.h"
#include "audio/Dsp.h"
#include "core/HelpTexts.h"
#include "midi/MidiInputManager.h"
#include "synth/SynthModule.h"

#include <limits>

using namespace kb;

namespace
{
class FixedModule : public StudioModule
{
public:
    explicit FixedModule (float valueToOutput) : value (valueToOutput) {}

    juce::String getId() const override { return "fast"; }
    juce::String getName() const override { return "Fast"; }
    void prepare (double, int) override {}
    void process (float* left, float* right, int numSamples, const MidiEvent*, int) noexcept override
    {
        std::fill (left, left + numSamples, value);
        std::fill (right, right + numSamples, value);
    }
    void allNotesOff() noexcept override {}
    std::vector<const Parameter*> getParameters() const override { return {}; }
    std::unique_ptr<juce::Component> createPanel() override { return nullptr; }

private:
    float value;
};

struct EngineRig
{
    explicit EngineRig (bool withSynth = true)
        : settings { juce::File() }, engine (hub, settings, registryWithCore())
    {
        if (withSynth)
            engine.addModule (std::make_unique<SynthModule> (registry, &hub.counters, &hub.state));
    }

    ParameterRegistry& registryWithCore()
    {
        registerCoreParameters (registry);
        return registry;
    }

    void prepare (double rate = 48000.0, int block = 256)
    {
        sampleRate = rate;
        engine.prepareForTesting (rate, block);
        left.assign (static_cast<size_t> (AudioEngine::maxBlockSize * 2), 0.0f);
        right.assign (left.size(), 0.0f);
    }

    float render (int numSamples)
    {
        engine.renderForTesting (left.data(), right.data(), numSamples);
        return std::max (kbtest::peakOf (left, numSamples), kbtest::peakOf (right, numSamples));
    }

    void noteOn (int note, int velocity)
    {
        const auto e = MidiEvent::noteOn (note, velocity);
        hub.state.apply (e);
        hub.counters.midiNoteOnReceived.fetch_add (1);
        hub.fifo.push (e);
    }

    void noteOff (int note)
    {
        const auto e = MidiEvent::noteOff (note);
        hub.state.apply (e);
        hub.counters.midiNoteOffReceived.fetch_add (1);
        hub.fifo.push (e);
    }

    MidiHub hub;
    ParameterRegistry registry;
    Settings settings;
    AudioEngine engine;
    double sampleRate = 48000.0;
    std::vector<float> left, right;
};
} // namespace

KB_TEST (Engine, note_through_the_queue_makes_sound_and_counters_agree)
{
    EngineRig rig;
    rig.prepare();
    rig.registry.require ("synth_attack").set (0.002f);

    KB_CHECK_EQ (rig.render (256), 0.0f);

    rig.noteOn (60, 110);
    rig.noteOn (64, 100);
    rig.noteOn (67, 90);

    float peak = 0.0f;
    for (int i = 0; i < 40; ++i)
        peak = std::max (peak, rig.render (256));

    KB_CHECK (peak > 0.005f);
    KB_CHECK (peak <= 1.0f);
    KB_CHECK (kbtest::allFinite (rig.left, 256));
    KB_CHECK_EQ (rig.hub.counters.activeVoices.load(), 3);
    KB_CHECK_EQ (rig.hub.counters.noteOnProcessed.load(), rig.hub.counters.midiNoteOnReceived.load());
    KB_CHECK (rig.engine.getMeter().getPeak (0) > 0.0f);

    rig.noteOff (60);
    rig.noteOff (64);
    rig.noteOff (67);
    rig.registry.require ("synth_release").set (0.1f);
    for (int i = 0; i < 400; ++i)
        rig.render (256);

    KB_CHECK_EQ (rig.hub.counters.activeVoices.load(), 0);
    KB_CHECK_EQ (rig.hub.counters.noteOffProcessed.load(), rig.hub.counters.midiNoteOffReceived.load());
    KB_CHECK_EQ (rig.render (256), 0.0f);
}

KB_TEST (Engine, master_volume_scales_the_output)
{
    EngineRig rig (false);
    rig.engine.addModule (std::make_unique<FixedModule> (0.5f));
    rig.prepare();

    rig.registry.require ("master").set (0.0f);
    for (int i = 0; i < 10; ++i)
        rig.render (256);
    const float full = rig.render (256);

    rig.registry.require ("master").set (-12.0f);
    for (int i = 0; i < 20; ++i)
        rig.render (256);
    const float quieter = rig.render (256);

    KB_CHECK_NEAR (full, 0.5, 1.0e-4);
    KB_CHECK_NEAR (quieter / full, dbToGain (-12.0f), 0.01);

    rig.registry.require ("master").set (-60.0f);
    for (int i = 0; i < 40; ++i)
        rig.render (256);
    KB_CHECK_EQ (rig.render (256), 0.0f);
}

KB_TEST (Engine, safe_start_volume_is_minus_12_db)
{
    EngineRig rig (false);
    KB_CHECK_NEAR (rig.registry.require ("master").get(), -12.0, 1.0e-6);
}

KB_TEST (Engine, limiter_and_clip_lamp_catch_loud_signals)
{
    EngineRig rig (false);
    rig.engine.addModule (std::make_unique<FixedModule> (3.0f));
    rig.prepare();
    rig.registry.require ("master").set (0.0f);

    KB_CHECK (! rig.engine.getMeter().isClipped());
    for (int i = 0; i < 10; ++i)
        KB_CHECK (rig.render (256) <= 1.0f);

    KB_CHECK (rig.engine.getMeter().isClipped());
    rig.registry.require ("master").set (-60.0f);
    for (int i = 0; i < 30; ++i)
        rig.render (256);
    rig.engine.getMeter().resetClip();
    KB_CHECK (! rig.engine.getMeter().isClipped());
    for (int i = 0; i < 10; ++i)
        rig.render (256);
    KB_CHECK (! rig.engine.getMeter().isClipped());
}

KB_TEST (Engine, nan_and_infinity_from_a_module_never_reach_the_output)
{
    for (const float bad : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                             -std::numeric_limits<float>::infinity() })
    {
        EngineRig rig (false);
        rig.engine.addModule (std::make_unique<FixedModule> (bad));
        rig.prepare();
        const float peak = rig.render (256);
        KB_CHECK (kbtest::allFinite (rig.left, 256));
        KB_CHECK (kbtest::allFinite (rig.right, 256));
        KB_CHECK (peak <= 1.0f);
    }
}

KB_TEST (Engine, panic_silences_synth_and_test_tone)
{
    EngineRig rig;
    rig.prepare();

    for (int i = 0; i < 8; ++i)
        rig.noteOn (40 + i * 4, 120);
    rig.engine.setTestToneEnabled (true);
    for (int i = 0; i < 20; ++i)
        rig.render (256);
    KB_CHECK (rig.engine.isTestToneEnabled());
    KB_CHECK_EQ (rig.hub.counters.activeVoices.load(), 8);

    rig.engine.panic();
    KB_CHECK_EQ (rig.hub.state.countKeysDown(), 0);
    for (int i = 0; i < 20; ++i)
        rig.render (256);

    KB_CHECK (! rig.engine.isTestToneEnabled());
    KB_CHECK_EQ (rig.hub.counters.activeVoices.load(), 0);
    KB_CHECK_EQ (rig.render (256), 0.0f);
}

KB_TEST (Engine, test_tone_is_audible_at_the_expected_level)
{
    EngineRig rig (false);
    rig.prepare();
    rig.registry.require ("master").set (0.0f);
    rig.engine.setTestToneEnabled (true);

    float peak = 0.0f;
    for (int i = 0; i < 20; ++i)
        peak = std::max (peak, rig.render (256));

    KB_CHECK_NEAR (peak, dbToGain (-18.0f), 0.003);
}

KB_TEST (Engine, huge_blocks_are_split_safely)
{
    EngineRig rig;
    rig.prepare (48000.0, 256);
    rig.noteOn (60, 100);
    rig.noteOn (72, 100);

    const float peak = rig.render (AudioEngine::maxBlockSize * 2);
    KB_CHECK (kbtest::allFinite (rig.left, AudioEngine::maxBlockSize * 2));
    KB_CHECK (peak > 0.0f);
    KB_CHECK (peak <= 1.0f);
}

KB_TEST (Engine, more_events_than_fit_in_one_block_are_all_processed_in_later_blocks)
{
    EngineRig rig;
    rig.prepare();
    rig.registry.require ("synth_release").set (0.005f);

    int pairs = 0;
    for (int i = 0; i < 1000; ++i)
    {
        rig.noteOn (30 + i % 60, 100);
        rig.noteOff (30 + i % 60);
        ++pairs;
    }
    KB_CHECK_EQ (rig.hub.fifo.getDroppedCount(), 0u);

    for (int i = 0; i < 10; ++i)
        rig.render (256);

    KB_CHECK_EQ (rig.hub.counters.noteOnProcessed.load(), static_cast<std::uint32_t> (pairs));
    KB_CHECK_EQ (rig.hub.counters.noteOffProcessed.load(), static_cast<std::uint32_t> (pairs));
    KB_CHECK_EQ (rig.hub.fifo.getNumReady(), 0);

    for (int i = 0; i < 100; ++i)
        rig.render (256);
    KB_CHECK_EQ (rig.hub.counters.activeVoices.load(), 0);
}

KB_TEST (Engine, two_modules_are_summed)
{
    EngineRig rig (false);
    rig.engine.addModule (std::make_unique<FixedModule> (0.1f));
    rig.engine.addModule (std::make_unique<FixedModule> (0.2f));
    rig.prepare();
    rig.registry.require ("master").set (0.0f);

    for (int i = 0; i < 10; ++i)
        rig.render (256);
    KB_CHECK_NEAR (rig.render (256), 0.3, 1.0e-4);
}

KB_TEST (Engine, works_at_44100_with_odd_block_sizes)
{
    EngineRig rig;
    rig.prepare (44100.0, 64);
    rig.noteOn (60, 100);

    for (const int n : { 1, 7, 64, 100, 333, 1024 })
    {
        rig.render (n);
        KB_CHECK (kbtest::allFinite (rig.left, n));
    }
    KB_CHECK_EQ (rig.hub.counters.activeVoices.load(), 1);
}

KB_TEST (Engine, starts_and_stops_without_audio_hardware_and_never_crashes)
{
    EngineRig rig;
    rig.engine.start();

    KB_CHECK (rig.engine.getStatusText().isNotEmpty());
    KB_CHECK (rig.engine.getBufferLatencyMs() >= 0.0);
    KB_CHECK (rig.engine.getCpuUsage() >= 0.0f);

    rig.engine.selectDriver ("Finnes ikke");
    rig.engine.selectOutput ("Finnes heller ikke");
    rig.engine.selectSampleRate (44100.0);
    rig.engine.selectBufferSize (128);
    rig.engine.reconnectNow();
    rig.engine.panic();
    KB_CHECK (rig.engine.getStatusText().isNotEmpty());

    rig.engine.shutdown();
    rig.engine.shutdown();
}

KB_TEST (Midi, manager_starts_rescans_and_stops_without_midi_devices)
{
    MidiHub hub;
    Settings settings { juce::File() };
    MidiInputManager manager (hub, settings);

    manager.start();
    manager.rescan();
    KB_CHECK (manager.getStatusText().isNotEmpty());

    if (manager.getDevices().empty())
        KB_CHECK (manager.getTracker().colour() != StatusColour::Green);

    manager.shutdown();
    manager.shutdown();
}
