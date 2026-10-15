#include "SynthRig.h"

#include "audio/AudioEngine.h"
#include "core/HelpTexts.h"
#include "midi/MidiHub.h"
#include "midi/MidiInputManager.h"

#include <atomic>
#include <chrono>
#include <array>
#include <string>
#include <thread>

using namespace kb;
using kbtest::Rng;
using kbtest::SynthRig;

/** Tilfeldige "hengende tone"-tester.

    Hver kjøring spiller en akkord (eller flere) på et simulert piano og slipper alt igjen.
    Testen later som den er MIDI-tråden: tangenttilstanden (MidiState) oppdateres med en gang, mens
    hendelsene til synthen kan komme senere, bli forsinket én blokk, ligge på samme sample, eller
    gå helt tapt. Når alle tangenter er sluppet og pedalen er oppe, SKAL synthen bli stille
    innen en fast tid, og ingen stemme får stå igjen. */
namespace
{
struct Config
{
    bool useKeyState = true;
    bool allowDrops = true;
    bool allowOffsets = true;
    int forcedChordSize = 0;
    int forcedPedal = -1;
    bool dropAllNoteOffs = false;
};

struct Stats
{
    int scenarios = 0;
    int failures = 0;
    long noteOns = 0;
    long noteOffsDelivered = 0;
    long eventsDropped = 0;
    long watchdogReleases = 0;
    long blocks = 0;
    long stolenScenarios = 0;
    double worstSilenceFraction = 0.0;
    std::string firstFailure;
};

const std::vector<float> attacks { 0.001f, 0.005f, 0.05f, 0.3f };
const std::vector<float> decays { 0.001f, 0.05f, 0.5f, 2.0f };
const std::vector<float> sustains { 0.0f, 0.3f, 0.8f, 1.0f };
const std::vector<float> releases { 0.005f, 0.02f, 0.1f, 0.3f, 0.6f, 1.0f };
const std::vector<int> chordSizes { 1, 2, 3, 5, 8, 9, 12 };
const std::vector<int> blockSizes { 16, 32, 64, 128, 256, 512 };
const std::vector<double> offDrops { 0.0, 0.0, 0.1, 0.3, 0.5, 1.0 };
const std::vector<double> onDrops { 0.0, 0.0, 0.0, 0.05 };
const std::vector<double> pedalDrops { 0.0, 0.0, 0.5, 1.0 };

class Scenario
{
public:
    Scenario (unsigned seedToUse, const Config& configToUse, Stats& statsToUpdate)
        : seed (seedToUse),
          cfg (configToUse),
          stats (statsToUpdate),
          rng (seedToUse),
          rig (rng.chance (0.5) ? 48000.0 : 44100.0, 1024, configToUse.useKeyState)
    {
    }

    bool run()
    {
        configure();
        playPhase();
        if (failed)
            return false;
        finishPhase();
        return ! failed;
    }

    std::string describe() const
    {
        return "frø " + std::to_string (seed) + ", akkord " + std::to_string (keys.size()) + " tangenter, pedal "
               + (usePedal ? "ja" : "nei") + ", release " + std::to_string (release) + " s, tapte NoteOff "
               + std::to_string (dropOff) + ", tapte pedal-opp " + std::to_string (dropPedalUp) + ", offset "
               + (useOffsets ? "ja" : "nei");
    }

    std::string reason;

private:
    void configure()
    {
        rig.setParam ("synth_attack", rng.pick (attacks));
        rig.setParam ("synth_decay", rng.pick (decays));
        rig.setParam ("synth_sustain", rng.pick (sustains));
        release = rng.chance (0.03) ? 2.0f : rng.pick (releases);
        rig.setParam ("synth_release", release);
        rig.setParam ("synth_detune", static_cast<float> (rng.range (0, 50)));
        rig.setParam ("synth_volume", static_cast<float> (0.3 + 0.7 * rng.unit()));

        const int size = cfg.forcedChordSize > 0 ? cfg.forcedChordSize : rng.pick (chordSizes);
        const bool clustered = rng.chance (0.5);
        const int base = rng.range (21, 84);

        while (static_cast<int> (keys.size()) < size)
        {
            const int note = clustered ? juce::jlimit (21, 108, base + rng.range (0, 24)) : rng.range (21, 108);
            if (std::find (keys.begin(), keys.end(), note) == keys.end())
                keys.push_back (note);
            else if (rng.chance (0.02))
                break;
        }

        usePedal = cfg.forcedPedal >= 0 ? cfg.forcedPedal == 1 : rng.chance (0.5);
        useOffsets = cfg.allowOffsets && rng.chance (0.5);

        dropOff = cfg.dropAllNoteOffs ? 1.0 : (cfg.allowDrops ? rng.pick (offDrops) : 0.0);
        dropOn = cfg.allowDrops ? rng.pick (onDrops) : 0.0;
        dropPedalUp = cfg.allowDrops ? rng.pick (pedalDrops) : 0.0;
        delayChance = 0.3;
    }

    void pushEvent (MidiEvent event, bool delivered, int& offsetCursor, int blockSize, std::vector<MidiEvent>& fresh)
    {
        if (! delivered)
        {
            ++stats.eventsDropped;
            return;
        }

        if (useOffsets)
        {
            offsetCursor = std::min (blockSize - 1, offsetCursor + (rng.chance (0.4) ? 0 : rng.range (0, blockSize / 4)));
            event.sampleOffset = offsetCursor;
        }
        else
        {
            event.sampleOffset = 0;
        }

        fresh.push_back (event);
    }

    void doPress (int note, int blockSize, int& cursor, std::vector<MidiEvent>& fresh)
    {
        const int velocity = rng.range (1, 127);
        auto e = rig.press (note, velocity);
        ++stats.noteOns;
        down[static_cast<size_t> (note)] = true;
        pushEvent (e, ! rng.chance (dropOn), cursor, blockSize, fresh);
    }

    void doRelease (int note, int blockSize, int& cursor, std::vector<MidiEvent>& fresh)
    {
        auto e = rig.release (note);
        down[static_cast<size_t> (note)] = false;
        const bool delivered = ! rng.chance (dropOff);
        if (delivered)
            ++stats.noteOffsDelivered;
        pushEvent (e, delivered, cursor, blockSize, fresh);
    }

    void doPedal (bool pedalDown, int blockSize, int& cursor, std::vector<MidiEvent>& fresh)
    {
        auto e = rig.pedal (pedalDown);
        pedalIsDown = pedalDown;
        pushEvent (e, pedalDown ? true : ! rng.chance (dropPedalUp), cursor, blockSize, fresh);
    }

    void processBlock (int blockSize, std::vector<MidiEvent>& fresh, bool delayFresh)
    {
        std::vector<MidiEvent> toDeliver;

        for (auto e : pending)
        {
            e.sampleOffset = 0;
            toDeliver.push_back (e);
        }
        pending.clear();

        if (delayFresh)
        {
            for (auto e : fresh)
                pending.push_back (e);
        }
        else
        {
            for (auto e : fresh)
                toDeliver.push_back (e);
        }
        fresh.clear();

        rig.block (toDeliver, blockSize);
        ++stats.blocks;
        checkInvariants();
    }

    void checkInvariants()
    {
        if (failed)
            return;

        auto fail = [this] (const std::string& why)
        {
            failed = true;
            reason = why;
        };

        if (rig.nonFiniteBlocks > 0)
            return fail ("NaN eller uendelig i lydutgangen");
        if (rig.maxPeak > 2.0f)
            return fail ("utgangen gikk over 2.0: " + std::to_string (rig.maxPeak));
        if (rig.synth.getActiveVoices() > SynthModule::numVoices)
            return fail ("flere aktive stemmer enn mulig");

        if (cfg.useKeyState)
        {
            if (rig.synth.getNumGatedVoices() > rig.state.countKeysDown())
                return fail ("en stemme har gate på for en tangent som ikke er nede (hengende tone)");
            if (! rig.state.isSustainDown() && rig.synth.getNumSustainedVoices() > 0)
                return fail ("en stemme holdes av pedalen selv om pedalen er oppe (hengende tone)");
        }
    }

    void playPhase()
    {
        const int playBlocks = rng.range (2, 30);
        size_t nextToPress = 0;
        std::vector<int> held;
        const int pedalDownBlock = usePedal ? rng.range (0, playBlocks / 2) : -1;
        const int pedalUpBlock = usePedal ? rng.range (pedalDownBlock, playBlocks) : -1;
        const bool chordAtOnce = rng.chance (0.3);

        for (int b = 0; b < playBlocks && ! failed; ++b)
        {
            const int blockSize = rng.pick (blockSizes);
            std::vector<MidiEvent> fresh;
            int cursor = 0;

            if (b == pedalDownBlock)
                doPedal (true, blockSize, cursor, fresh);

            if (nextToPress < keys.size() && (b == 0 || rng.chance (0.6)))
            {
                const size_t count = chordAtOnce ? keys.size() - nextToPress
                                                 : static_cast<size_t> (rng.range (1, static_cast<int> (keys.size())));
                for (size_t i = 0; i < count && nextToPress < keys.size(); ++i)
                {
                    const int note = keys[nextToPress++];
                    if (chordAtOnce)
                        cursor = 0;
                    doPress (note, blockSize, cursor, fresh);
                    held.push_back (note);
                }
            }

            if (! held.empty() && rng.chance (0.1))
                doPress (rng.pick (held), blockSize, cursor, fresh);

            if (! held.empty() && rng.chance (0.3))
            {
                const int count = rng.range (1, static_cast<int> (held.size()));
                for (int i = 0; i < count && ! held.empty(); ++i)
                {
                    const size_t index = rng.next() % held.size();
                    const int note = held[index];
                    held.erase (held.begin() + static_cast<long> (index));
                    doRelease (note, blockSize, cursor, fresh);

                    if (rng.chance (0.15))
                    {
                        doPress (note, blockSize, cursor, fresh);
                        held.push_back (note);
                    }
                }
            }

            if (b == pedalUpBlock && pedalIsDown)
                doPedal (false, blockSize, cursor, fresh);

            processBlock (blockSize, fresh, rng.chance (delayChance));
        }

        {
            const int blockSize = rng.pick (blockSizes);
            std::vector<MidiEvent> fresh;
            int cursor = 0;
            const bool pedalFirst = rng.chance (0.5);

            if (pedalFirst && pedalIsDown)
                doPedal (false, blockSize, cursor, fresh);

            while (! held.empty())
            {
                const size_t index = rng.next() % held.size();
                const int note = held[index];
                held.erase (held.begin() + static_cast<long> (index));
                doRelease (note, blockSize, cursor, fresh);
            }

            if (! pedalFirst && pedalIsDown)
                doPedal (false, blockSize, cursor, fresh);

            processBlock (blockSize, fresh, false);
        }
    }

    void finishPhase()
    {
        const double rate = rig.rate;
        const double bound = 2.0 * static_cast<double> (release) + 0.1 + 0.15;
        const int blockSize = 128;
        const int maxBlocks = static_cast<int> (bound * rate / blockSize) + 2;

        int blocksUsed = 0;
        while (rig.synth.getActiveVoices() > 0 && blocksUsed < maxBlocks && ! failed)
        {
            std::vector<MidiEvent> none;
            processBlock (blockSize, none, false);
            ++blocksUsed;
        }

        if (failed)
            return;

        const double used = blocksUsed * blockSize / rate;
        stats.worstSilenceFraction = std::max (stats.worstSilenceFraction, used / bound);

        if (rig.synth.getActiveVoices() != 0)
        {
            failed = true;
            reason = "synthen ble ikke stille innen " + std::to_string (bound) + " s etter at alt ble sluppet ("
                     + std::to_string (rig.synth.getActiveVoices()) + " aktive stemmer, "
                     + std::to_string (rig.synth.getNumGatedVoices()) + " med gate, "
                     + std::to_string (rig.synth.getNumSustainedVoices()) + " med pedal)";
            return;
        }

        if (rig.synth.getNumGatedVoices() != 0 || rig.synth.getNumSustainedVoices() != 0)
        {
            failed = true;
            reason = "stille, men en stemme har fortsatt gate eller pedal";
            return;
        }

        std::vector<MidiEvent> none;
        const float tail = rig.block (none, 256);
        if (tail != 0.0f)
        {
            failed = true;
            reason = "synthen sier 0 aktive stemmer, men gir fortsatt lyd: " + std::to_string (tail);
        }

        stats.watchdogReleases += rig.counters.watchdogReleases.load();
    }

    unsigned seed;
    Config cfg;
    Stats& stats;
    Rng rng;
    SynthRig rig;

    std::vector<int> keys;
    std::array<bool, 128> down {};
    std::vector<MidiEvent> pending;
    bool usePedal = false;
    bool useOffsets = false;
    bool pedalIsDown = false;
    bool failed = false;
    float release = 0.3f;
    double dropOff = 0.0, dropOn = 0.0, dropPedalUp = 0.0, delayChance = 0.3;
};

Stats runMany (unsigned firstSeed, int count, const Config& config)
{
    Stats stats;
    for (int i = 0; i < count; ++i)
    {
        Scenario scenario (firstSeed + static_cast<unsigned> (i), config, stats);
        ++stats.scenarios;

        if (! scenario.run())
        {
            ++stats.failures;
            if (stats.firstFailure.empty())
                stats.firstFailure = scenario.describe() + ": " + scenario.reason;
        }
    }
    return stats;
}

void report (const char* name, const Stats& s)
{
    std::printf ("    %s: %d kjøringer, %ld NoteOn, %ld NoteOff levert, %ld hendelser tapt, "
                 "%ld sikkerhetsnett-slipp, %ld lydblokker, lengste tid til stillhet %.0f %% av grensen, %d feil\n",
                 name, s.scenarios, s.noteOns, s.noteOffsDelivered, s.eventsDropped, s.watchdogReleases, s.blocks,
                 100.0 * s.worstSilenceFraction, s.failures);
    if (s.failures > 0)
        std::printf ("    første feil: %s\n", s.firstFailure.c_str());
}
} // namespace

KB_TEST (Stress, random_chords_with_lost_events_and_watchdog_never_leave_stuck_notes)
{
    const auto stats = runMany (1, 1500, Config {});
    report ("tilfeldige akkorder med tapte hendelser", stats);
    KB_CHECK_EQ (stats.failures, 0);
    KB_CHECK (stats.scenarios >= 1000);
    KB_CHECK (stats.eventsDropped > 1000);
    KB_CHECK (stats.watchdogReleases > 100);
}

KB_TEST (Stress, event_logic_alone_without_watchdog_has_no_stuck_notes)
{
    Config config;
    config.useKeyState = false;
    config.allowDrops = false;

    const auto stats = runMany (100000, 1000, config);
    report ("stemmelogikk uten sikkerhetsnett og uten tap", stats);
    KB_CHECK_EQ (stats.failures, 0);
    KB_CHECK_EQ (stats.watchdogReleases, 0L);
}

KB_TEST (Stress, every_chord_size_with_and_without_pedal)
{
    for (const int size : { 2, 3, 5, 8, 9, 12 })
    {
        for (const int pedal : { 0, 1 })
        {
            Config config;
            config.forcedChordSize = size;
            config.forcedPedal = pedal;

            const std::string name = std::to_string (size) + " tangenter, pedal " + (pedal ? "ja" : "nei");
            const auto stats = runMany (200000 + static_cast<unsigned> (size * 1000 + pedal * 500), 120, config);
            report (name.c_str(), stats);
            KB_CHECK_MSG (stats.failures == 0, name + ": " + stats.firstFailure);
        }
    }
}

KB_TEST (Stress, all_note_offs_lost_is_still_silent_thanks_to_the_watchdog)
{
    Config config;
    config.dropAllNoteOffs = true;
    config.forcedPedal = 0;

    const auto stats = runMany (300000, 300, config);
    report ("alle NoteOff tapt", stats);
    KB_CHECK_EQ (stats.failures, 0);
    KB_CHECK (stats.watchdogReleases > 200);
}

KB_TEST (Stress, without_the_key_state_a_lost_note_off_really_does_hang)
{
    // Kontrolltest: beviser at testene faktisk kan fange feilen. Uten tangenttilstanden
    // (sikkerhetsnettet) blir en tapt NoteOff stående som en hengende tone.
    Config config;
    config.useKeyState = false;
    config.dropAllNoteOffs = true;
    config.forcedPedal = 0;

    const auto stats = runMany (400000, 50, config);
    report ("kontroll: sikkerhetsnettet er av", stats);
    KB_CHECK (stats.failures > 40);
}

KB_TEST (Stress, two_keys_at_the_same_time_the_original_bug)
{
    for (const bool useState : { true, false })
    {
        for (const int releaseOrder : { 0, 1, 2 })
        {
            SynthRig rig (48000.0, 1024, useState);
            rig.setParam ("synth_release", 0.3f);

            std::vector<MidiEvent> events { rig.press (60, 100, 0), rig.press (64, 90, 0) };
            rig.block (events, 128);
            rig.run (0.3);
            KB_CHECK_EQ (rig.synth.getActiveVoices(), 2);

            std::vector<MidiEvent> offs;
            if (releaseOrder == 0)
                offs = { rig.release (60, 5), rig.release (64, 5) };
            else if (releaseOrder == 1)
                offs = { rig.release (64, 0), rig.release (60, 1) };
            else
                offs = { rig.release (60, 0), rig.release (64, 127) };

            rig.block (offs, 128);
            KB_CHECK_EQ (rig.synth.getNumGatedVoices(), 0);
            rig.run (1.0);
            KB_CHECK_EQ (rig.synth.getActiveVoices(), 0);
            KB_CHECK_EQ (rig.block (128), 0.0f);
        }
    }
}

KB_TEST (Stress, note_on_and_note_off_in_the_same_block_for_many_keys)
{
    for (const int count : { 2, 3, 5, 8, 9, 12, 20 })
    {
        for (const bool useState : { true, false })
        {
            SynthRig rig (48000.0, 1024, useState);
            rig.setParam ("synth_release", 0.1f);

            std::vector<MidiEvent> events;
            for (int i = 0; i < count; ++i)
                events.push_back (rig.press (40 + i * 3, 100, 10));
            for (int i = count - 1; i >= 0; --i)
                events.push_back (rig.release (40 + i * 3, 20));

            rig.block (events, 128);
            rig.run (0.8);

            KB_CHECK_MSG (rig.synth.getActiveVoices() == 0, "tangenter: " + std::to_string (count));
            KB_CHECK_EQ (rig.synth.getNumGatedVoices(), 0);
        }
    }
}

KB_TEST (Stress, midi_disconnect_silences_held_notes_even_when_every_event_is_lost)
{
    SynthRig rig;
    rig.setParam ("synth_release", 0.2f);

    std::vector<MidiEvent> events;
    for (int i = 0; i < 6; ++i)
        events.push_back (rig.press (50 + i, 100));
    events.push_back (rig.pedal (true));
    rig.block (events, 256);
    rig.run (0.2);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 6);

    // Pianoet kobles fra: ingen NoteOff og ingen AllNotesOff kommer frem til lydtråden.
    rig.state.reset();
    rig.block (256);
    KB_CHECK_EQ (rig.synth.getNumGatedVoices(), 0);
    KB_CHECK_EQ (rig.synth.getNumSustainedVoices(), 0);
    rig.run (1.0);
    KB_CHECK_EQ (rig.synth.getActiveVoices(), 0);
}

KB_TEST (Stress, midi_disconnect_helper_resets_state_and_sends_all_notes_off)
{
    MidiHub hub;
    hub.state.apply (MidiEvent::noteOn (60, 100));
    hub.state.apply (MidiEvent::noteOn (64, 100));
    hub.state.apply (MidiEvent::sustain (true));
    hub.state.apply (MidiEvent::modWheel (50));
    KB_CHECK_EQ (hub.state.countKeysDown(), 2);

    MidiInputManager::releaseEverything (hub);

    KB_CHECK_EQ (hub.state.countKeysDown(), 0);
    KB_CHECK (! hub.state.isSustainDown());
    KB_CHECK_EQ (hub.state.getModWheel(), 0);

    MidiEvent out[4];
    KB_CHECK_EQ (hub.fifo.pop (out, 4), 1);
    KB_CHECK (out[0].type == MidiEventType::AllNotesOff);
}

KB_TEST (Stress, disconnect_with_full_queue_still_ends_silent)
{
    MidiHub hub;
    ParameterRegistry registry;
    registerCoreParameters (registry);
    Settings settings { juce::File() };
    AudioEngine engine (hub, settings, registry);
    engine.addModule (std::make_unique<SynthModule> (registry, &hub.counters, &hub.state));
    engine.prepareForTesting (48000.0, 256);
    registry.require ("synth_release").set (0.2f);

    std::vector<float> left (256), right (256);
    for (int i = 0; i < 5; ++i)
    {
        hub.state.apply (MidiEvent::noteOn (50 + i, 100));
        hub.fifo.push (MidiEvent::noteOn (50 + i, 100));
    }
    engine.renderForTesting (left.data(), right.data(), 256);
    KB_CHECK_EQ (hub.counters.activeVoices.load(), 5);

    for (int i = 0; i < 6000; ++i)
        hub.fifo.push (MidiEvent::modWheel (i % 100));
    KB_CHECK (hub.fifo.getDroppedCount() > 0);

    MidiInputManager::releaseEverything (hub);

    for (int i = 0; i < 400; ++i)
        engine.renderForTesting (left.data(), right.data(), 256);

    KB_CHECK_EQ (hub.counters.activeVoices.load(), 0);
}

KB_TEST (Stress, threads_midi_thread_against_audio_thread_with_flood_and_lost_events)
{
    MidiHub hub;
    ParameterRegistry registry;
    registerCoreParameters (registry);
    Settings settings { juce::File() };
    AudioEngine engine (hub, settings, registry);
    engine.addModule (std::make_unique<SynthModule> (registry, &hub.counters, &hub.state));
    engine.prepareForTesting (48000.0, 256);
    registry.require ("synth_release").set (0.15f);
    registry.require ("synth_attack").set (0.002f);

    std::atomic<bool> stop { false };
    std::atomic<bool> pauseAudio { false };
    std::atomic<bool> audioPaused { false };
    std::atomic<long> blocksRendered { 0 };
    std::atomic<long> badBlocks { 0 };
    std::atomic<long> overloadedBlocks { 0 };

    std::thread audioThread ([&]
    {
        std::vector<float> left (256), right (256);
        while (! stop.load())
        {
            if (pauseAudio.load())
            {
                audioPaused = true;
                std::this_thread::sleep_for (std::chrono::microseconds (200));
                continue;
            }
            audioPaused = false;

            engine.renderForTesting (left.data(), right.data(), 256);
            ++blocksRendered;

            for (int i = 0; i < 256; ++i)
                if (! std::isfinite (left[static_cast<size_t> (i)]) || std::fabs (left[static_cast<size_t> (i)]) > 1.0f
                    || ! std::isfinite (right[static_cast<size_t> (i)]) || std::fabs (right[static_cast<size_t> (i)]) > 1.0f)
                {
                    ++badBlocks;
                    break;
                }

            if (hub.counters.activeVoices.load() > SynthModule::numVoices)
                ++overloadedBlocks;

            std::this_thread::yield();
        }
    });

    Rng rng (2024);
    std::vector<int> downNotes;
    auto sendOn = [&] (int note, int velocity)
    {
        const auto e = MidiEvent::noteOn (note, velocity);
        hub.state.apply (e);
        hub.counters.midiNoteOnReceived.fetch_add (1);
        hub.fifo.push (e);
    };
    auto sendOff = [&] (int note, bool lose)
    {
        const auto e = MidiEvent::noteOff (note);
        hub.state.apply (e);
        hub.counters.midiNoteOffReceived.fetch_add (1);
        if (! lose)
            hub.fifo.push (e);
    };

    const auto testStart = std::chrono::steady_clock::now();
    int rounds = 0;

    while (std::chrono::steady_clock::now() - testStart < std::chrono::milliseconds (2500))
    {
        ++rounds;
        const int chord = rng.pick (std::vector<int> { 1, 2, 3, 5, 8, 9, 12 });
        downNotes.clear();

        if (rng.chance (0.3))
            { const auto p = MidiEvent::sustain (true); hub.state.apply (p); hub.fifo.push (p); }

        for (int i = 0; i < chord; ++i)
        {
            const int note = rng.range (21, 108);
            sendOn (note, rng.range (1, 127));
            downNotes.push_back (note);
            if (rng.chance (0.3))
                std::this_thread::sleep_for (std::chrono::microseconds (rng.range (0, 300)));
        }

        std::this_thread::sleep_for (std::chrono::microseconds (rng.range (0, 4000)));

        while (! downNotes.empty())
        {
            const size_t index = rng.next() % downNotes.size();
            sendOff (downNotes[index], rng.chance (0.2));
            downNotes.erase (downNotes.begin() + static_cast<long> (index));
        }

        if (rng.chance (0.5))
        {
            const auto p = MidiEvent::sustain (false);
            hub.state.apply (p);
            if (! rng.chance (0.3))
                hub.fifo.push (p);
        }

        if (rounds % 40 == 0)
        {
            pauseAudio = true;
            while (! audioPaused.load())
                std::this_thread::sleep_for (std::chrono::microseconds (100));

            for (int i = 0; i < 6000; ++i)
            {
                const int note = rng.range (21, 108);
                if (rng.chance (0.5))
                    sendOn (note, 80);
                else
                    sendOff (note, false);
            }
            pauseAudio = false;
        }
    }

    hub.state.reset();
    hub.fifo.push (MidiEvent::sustain (false));

    const auto settleStart = std::chrono::steady_clock::now();
    while (hub.counters.activeVoices.load() > 0
           && std::chrono::steady_clock::now() - settleStart < std::chrono::seconds (5))
        std::this_thread::sleep_for (std::chrono::milliseconds (5));

    stop = true;
    audioThread.join();

    std::printf ("    %d runder fra MIDI-tråden, %ld lydblokker, %u hendelser tapt i køen, %u sikkerhetsnett-slipp\n",
                 rounds, blocksRendered.load(), hub.fifo.getDroppedCount(), hub.counters.watchdogReleases.load());

    KB_CHECK_EQ (hub.counters.activeVoices.load(), 0);
    KB_CHECK_EQ (badBlocks.load(), 0L);
    KB_CHECK_EQ (overloadedBlocks.load(), 0L);
    KB_CHECK (hub.fifo.getDroppedCount() > 0);
    KB_CHECK (blocksRendered.load() > 100);

    std::vector<float> left (256), right (256);
    engine.renderForTesting (left.data(), right.data(), 256);
    KB_CHECK_EQ (kbtest::peakOf (left, 256), 0.0f);
    KB_CHECK_EQ (kbtest::peakOf (right, 256), 0.0f);
}
