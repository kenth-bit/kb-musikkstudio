#include "TestFramework.h"
#include "TestUtil.h"

#include "core/ConnectionTracker.h"
#include "core/HelpTexts.h"
#include "core/ParameterRegistry.h"
#include "core/Settings.h"
#include "midi/MidiHub.h"
#include "midi/MidiInputManager.h"
#include "modules/ModuleRegistry.h"

using namespace kb;

// --- ConnectionTracker -------------------------------------------------------------------

KB_TEST (ConnectionTracker, starts_red_and_not_connected)
{
    ConnectionTracker tracker;
    KB_CHECK (! tracker.isConnected());
    KB_CHECK (tracker.colour() == StatusColour::Red);
}

KB_TEST (ConnectionTracker, lost_then_retrying_then_connected_gives_red_yellow_green)
{
    ConnectionTracker tracker;
    tracker.onConnected (1.0);
    KB_CHECK (tracker.colour() == StatusColour::Green);

    tracker.onLost (10.0);
    KB_CHECK (tracker.state() == ConnectionState::Lost);
    KB_CHECK (tracker.colour() == StatusColour::Red);

    tracker.onRetryStarted (12.0);
    KB_CHECK (tracker.state() == ConnectionState::Retrying);
    KB_CHECK (tracker.colour() == StatusColour::Yellow);
    KB_CHECK_EQ (tracker.attempts(), 1);

    tracker.onRetryFailed (12.5);
    KB_CHECK (tracker.colour() == StatusColour::Yellow);

    tracker.onRetryStarted (14.0);
    KB_CHECK_EQ (tracker.attempts(), 2);

    tracker.onConnected (15.0);
    KB_CHECK (tracker.colour() == StatusColour::Green);
    KB_CHECK_EQ (tracker.attempts(), 0);
    KB_CHECK_NEAR (tracker.lastOutageSeconds(), 5.0, 1.0e-9);
}

KB_TEST (ConnectionTracker, the_stripe_does_not_stay_red_after_replugging)
{
    // Den opprinnelige feilen: stripen ble stående rød etter at USB-kabelen var satt i igjen.
    ConnectionTracker tracker;
    tracker.onConnected (0.0);

    for (int cycle = 0; cycle < 50; ++cycle)
    {
        const double t = cycle * 100.0;
        tracker.onLost (t + 1.0);
        KB_CHECK (tracker.colour() == StatusColour::Red);

        for (int attempt = 0; attempt < 3; ++attempt)
        {
            KB_CHECK (tracker.shouldRetry (t + 2.0 + attempt * 3.0, 2.0));
            tracker.onRetryStarted (t + 2.0 + attempt * 3.0);
            KB_CHECK (tracker.colour() == StatusColour::Yellow);
            tracker.onRetryFailed (t + 2.1 + attempt * 3.0);
        }

        tracker.onConnected (t + 9.0);
        KB_CHECK (tracker.colour() == StatusColour::Green);
        KB_CHECK (tracker.isConnected());
    }
}

KB_TEST (ConnectionTracker, retry_interval_logic)
{
    ConnectionTracker tracker;
    tracker.onConnected (0.0);
    KB_CHECK (! tracker.shouldRetry (100.0, 2.0));

    tracker.onLost (10.0);
    KB_CHECK (tracker.shouldRetry (10.0, 2.0));
    tracker.onRetryStarted (10.0);
    KB_CHECK (! tracker.shouldRetry (11.0, 2.0));
    KB_CHECK (tracker.shouldRetry (12.0, 2.0));
}

KB_TEST (ConnectionTracker, lost_while_already_lost_keeps_the_original_outage_start)
{
    ConnectionTracker tracker;
    tracker.onConnected (0.0);
    tracker.onLost (10.0);
    tracker.onLost (20.0);
    tracker.onConnected (30.0);
    KB_CHECK_NEAR (tracker.lastOutageSeconds(), 20.0, 1.0e-9);
}

// --- ParameterRegistry -------------------------------------------------------------------

KB_TEST (ParameterRegistry, add_find_and_fallback)
{
    ParameterRegistry registry;
    auto& p = registry.addParameter ({ "test", "Test", "Gruppe", "Hjelp", 0.0f, 10.0f, 5.0f, 1.0f, ParamFormat::Plain });

    KB_CHECK (registry.find ("test") == &p);
    KB_CHECK (registry.find ("finnes_ikke") == nullptr);
    KB_CHECK_EQ (p.get(), 5.0f);

}

KB_TEST (ParameterRegistry, set_clamps_and_ignores_non_finite_values)
{
    ParameterRegistry registry;
    auto& p = registry.addParameter ({ "x", "X", "G", "H", -2.0f, 2.0f, 0.5f, 1.0f, ParamFormat::Plain });

    p.set (100.0f);
    KB_CHECK_EQ (p.get(), 2.0f);
    p.set (-100.0f);
    KB_CHECK_EQ (p.get(), -2.0f);
    p.set (1.0f);
    p.set (std::numeric_limits<float>::quiet_NaN());
    p.set (std::numeric_limits<float>::infinity());
    KB_CHECK_EQ (p.get(), 1.0f);
    p.reset();
    KB_CHECK_EQ (p.get(), 0.5f);
}

KB_TEST (ParameterRegistry, formats_in_norwegian_units)
{
    ParameterRegistry registry;
    auto& time = registry.addParameter ({ "t", "T", "G", "H", 0.001f, 5.0f, 0.5f, 0.3f, ParamFormat::Time });
    auto& pct = registry.addParameter ({ "p", "P", "G", "H", 0.0f, 1.0f, 0.7f, 1.0f, ParamFormat::Percent });
    auto& cents = registry.addParameter ({ "c", "C", "G", "H", 0.0f, 50.0f, 16.0f, 1.0f, ParamFormat::Cents });
    auto& db = registry.addParameter ({ "d", "D", "G", "H", -60.0f, 6.0f, -12.0f, 1.0f, ParamFormat::Decibel });

    KB_CHECK_EQ (time.format (0.45f), juce::String ("450 ms"));
    KB_CHECK_EQ (time.format (1.4f), juce::String ("1.40 s"));
    KB_CHECK_EQ (pct.format (0.7f), juce::String ("70 %"));
    KB_CHECK_EQ (cents.format (16.0f), juce::String ("16 cent"));
    KB_CHECK_EQ (db.format (-12.0f), juce::String ("-12.0 dB"));
    KB_CHECK_EQ (db.format (-60.0f), juce::String ("-inf dB"));
}

KB_TEST (ParameterRegistry, skewed_range_round_trips)
{
    ParameterRegistry registry;
    auto& p = registry.addParameter ({ "a", "A", "G", "H", 0.001f, 5.0f, 0.45f, 0.3f, ParamFormat::Time });

    for (float v = 0.001f; v <= 5.0f; v += 0.05f)
    {
        const float normalised = p.getRange().convertTo0to1 (v);
        KB_CHECK (normalised >= 0.0f && normalised <= 1.0f);
        KB_CHECK_NEAR (p.getRange().convertFrom0to1 (normalised), v, 1.0e-3);
    }
}

KB_TEST (ParameterRegistry, help_lookup_covers_parameters_and_controls)
{
    ParameterRegistry registry;
    registry.addParameter ({ "knob", "Knott", "G", "Forklaring av knott", 0.0f, 1.0f, 0.0f, 1.0f, ParamFormat::Plain });
    registry.addControlHelp ({ "button", "Knapp", "G", "Forklaring av knapp" });

    KB_CHECK_EQ (registry.getHelp ("knob"), juce::String ("Forklaring av knott"));
    KB_CHECK_EQ (registry.getHelp ("button"), juce::String ("Forklaring av knapp"));
    KB_CHECK (registry.getHelp ("ukjent").isEmpty());
    KB_CHECK_EQ (registry.getName ("button"), juce::String ("Knapp"));
    KB_CHECK_EQ (registry.getName ("ukjent"), juce::String ("ukjent"));
}

KB_TEST (ParameterRegistry, parameters_are_lock_free_atomics)
{
    KB_CHECK (std::atomic<float>::is_always_lock_free);
}

// --- Hjelpetekster -----------------------------------------------------------------------

namespace
{
struct FullRegistry
{
    FullRegistry()
    {
        registerCoreParameters (registry);
        registerBuiltInModules (modules);
        ModuleContext context { registry, hub };
        created = modules.createAll (context);
    }

    ParameterRegistry registry;
    MidiHub hub;
    ModuleRegistry modules;
    std::vector<std::unique_ptr<StudioModule>> created;
};
} // namespace

KB_TEST (HelpTexts, every_help_id_used_by_the_ui_has_a_norwegian_help_text)
{
    FullRegistry full;
    const char* ids[] = {
        HelpId::audioDriver, HelpId::audioOutput, HelpId::audioSampleRate, HelpId::audioBuffer, HelpId::audioLatency,
        HelpId::audioStatus, HelpId::audioReconnect, HelpId::midiDevice, HelpId::midiStatus, HelpId::midiRescan,
        HelpId::master, HelpId::meter, HelpId::clip, HelpId::testTone, HelpId::panic, HelpId::keyboard,
        HelpId::noteInfo, HelpId::velocity, HelpId::sustainPedal, HelpId::pitchBend, HelpId::modWheel, HelpId::log,
        HelpId::logDebug, HelpId::cpu, HelpId::panelConnection, HelpId::panelSynth, HelpId::panelMaster,
        HelpId::panelKeys, HelpId::panelLog, "synth_voices", "synth_envelope", "preset_pad", "preset_strings",
        "preset_pluck", "preset_bass"
    };

    for (const auto* id : ids)
    {
        const auto help = full.registry.getHelp (id);
        KB_CHECK_MSG (help.length() > 20, std::string ("mangler hjelpetekst: ") + id);
        KB_CHECK_MSG (full.registry.getName (id) != juce::String (id), std::string ("mangler navn: ") + id);
    }
}

KB_TEST (HelpTexts, every_parameter_has_name_group_and_help)
{
    FullRegistry full;
    const auto parameters = full.registry.allParameters();
    KB_CHECK (parameters.size() >= 7);

    for (const auto* p : parameters)
    {
        KB_CHECK_MSG (p->info().name.isNotEmpty(), p->info().id.toStdString());
        KB_CHECK_MSG (p->info().group.isNotEmpty(), p->info().id.toStdString());
        KB_CHECK_MSG (p->info().help.length() > 20, p->info().id.toStdString());
        KB_CHECK_MSG (p->info().help.startsWith (p->info().name), p->info().id.toStdString());
        KB_CHECK (p->info().minValue < p->info().maxValue);
        KB_CHECK (p->info().defaultValue >= p->info().minValue && p->info().defaultValue <= p->info().maxValue);
    }
}

KB_TEST (HelpTexts, texts_contain_no_mojibake)
{
    FullRegistry full;
    auto check = [] (const juce::String& text)
    {
        KB_CHECK_MSG (! text.contains (juce::String::fromUTF8 ("Ã")), text.toStdString());
        KB_CHECK_MSG (! text.contains (juce::String::fromUTF8 ("Â")), text.toStdString());
    };

    for (const auto* p : full.registry.allParameters())
        check (p->info().help);
    for (const auto& c : full.registry.allControls())
        check (c.help);
}

KB_TEST (HelpTexts, norwegian_letters_survive)
{
    FullRegistry full;
    KB_CHECK (full.registry.getHelp (HelpId::meter).contains (juce::String::fromUTF8 ("Måler")));
    KB_CHECK (full.registry.getHelp (HelpId::logDebug).contains (juce::String::fromUTF8 ("Feilsøking")));
}

KB_TEST (HelpTexts, core_registration_is_idempotent)
{
    ParameterRegistry registry;
    registerCoreParameters (registry);
    const auto count = registry.allControls().size();
    registerCoreParameters (registry);
    KB_CHECK_EQ (registry.allControls().size(), count);
    KB_CHECK_EQ (registry.allParameters().size(), static_cast<size_t> (1));
}

// --- Moduler -----------------------------------------------------------------------------

KB_TEST (ModuleRegistry, creates_the_synth_with_six_parameters)
{
    FullRegistry full;
    KB_CHECK_EQ (full.modules.getIds().size(), 1);
    KB_CHECK_EQ (full.modules.getIds()[0], juce::String ("synth"));
    KB_CHECK_EQ (full.created.size(), static_cast<size_t> (1));
    KB_CHECK_EQ (full.created[0]->getId(), juce::String ("synth"));
    KB_CHECK_EQ (full.created[0]->getParameters().size(), static_cast<size_t> (6));
}

KB_TEST (ModuleRegistry, a_new_module_can_be_registered_without_touching_the_core)
{
    struct Silent : StudioModule
    {
        juce::String getId() const override { return "stille"; }
        juce::String getName() const override { return "Stille"; }
        void prepare (double, int) override {}
        void process (float* l, float* r, int n, const MidiEvent*, int) noexcept override
        {
            std::fill (l, l + n, 0.0f);
            std::fill (r, r + n, 0.0f);
        }
        void allNotesOff() noexcept override {}
        std::vector<const Parameter*> getParameters() const override { return {}; }
        std::unique_ptr<juce::Component> createPanel() override { return nullptr; }
    };

    ParameterRegistry registry;
    MidiHub hub;
    ModuleRegistry modules;
    registerBuiltInModules (modules);
    modules.registerModule ("stille", [] (ModuleContext&) { return std::make_unique<Silent>(); });
    ModuleContext context { registry, hub };

    const auto created = modules.createAll (context);
    KB_CHECK_EQ (created.size(), static_cast<size_t> (2));
    KB_CHECK_EQ (created[1]->getId(), juce::String ("stille"));
}

// --- Settings ----------------------------------------------------------------------------

KB_TEST (Settings, missing_file_gives_defaults)
{
    kbtest::TempDir dir;
    Settings settings (dir.file ("finnes_ikke.json"));
    KB_CHECK (! settings.load());
    KB_CHECK_EQ (settings.getInt ("x", 42), 42);
    KB_CHECK_EQ (settings.getString ("navn", "standard"), juce::String ("standard"));
    KB_CHECK_NEAR (settings.getDouble ("d", 1.5), 1.5, 1.0e-12);
}

KB_TEST (Settings, corrupt_file_falls_back_to_defaults_and_keeps_a_copy)
{
    const std::vector<std::string> garbageFiles { "{ dette er ikke json", "", "[1,2,3]", "\"bare tekst\"", "{\"a\": ",
                                                  std::string ("\x01\x02\xff\xfe", 4), "{{{{", "null" };

    for (const auto& garbage : garbageFiles)
    {
        kbtest::TempDir dir;
        const auto file = dir.file ("innstillinger.json");
        file.replaceWithData (garbage.data(), garbage.size());

        Settings settings (file);
        KB_CHECK_MSG (! settings.load(), "fil: " + garbage);
        KB_CHECK_EQ (settings.getInt ("audio.bufferSize", 256), 256);
        KB_CHECK_EQ (settings.getString ("audio.driver"), juce::String());

        settings.setInt ("audio.bufferSize", 128);
        KB_CHECK (settings.save());

        Settings reloaded (file);
        KB_CHECK (reloaded.load());
        KB_CHECK_EQ (reloaded.getInt ("audio.bufferSize", 256), 128);
    }
}

KB_TEST (Settings, wrong_value_types_give_defaults)
{
    kbtest::TempDir dir;
    const auto file = dir.file ("s.json");
    file.replaceWithText ("{\"audio.bufferSize\": \"mye\", \"audio.driver\": 5, \"audio.sampleRate\": true}");

    Settings settings (file);
    KB_CHECK (settings.load());
    KB_CHECK_EQ (settings.getInt ("audio.bufferSize", 256), 256);
    KB_CHECK_EQ (settings.getString ("audio.driver", "ASIO"), juce::String ("ASIO"));
    KB_CHECK_NEAR (settings.getDouble ("audio.sampleRate", 48000.0), 48000.0, 1.0e-9);
}

KB_TEST (Settings, round_trip_keeps_values_and_norwegian_letters)
{
    kbtest::TempDir dir;
    const auto file = dir.file ("rund.json");

    {
        Settings settings (file);
        settings.setString ("midi.deviceName", juce::String::fromUTF8 ("Pianoet i stua på Ørland"));
        settings.setInt ("audio.bufferSize", 64);
        settings.setDouble ("audio.sampleRate", 44100.0);
        KB_CHECK (settings.save());
    }

    Settings settings (file);
    KB_CHECK (settings.load());
    KB_CHECK_EQ (settings.getString ("midi.deviceName"), juce::String::fromUTF8 ("Pianoet i stua på Ørland"));
    KB_CHECK_EQ (settings.getInt ("audio.bufferSize", 0), 64);
    KB_CHECK_NEAR (settings.getDouble ("audio.sampleRate", 0.0), 44100.0, 1.0e-9);
}

KB_TEST (Settings, save_without_file_is_harmless)
{
    Settings settings { juce::File() };
    settings.setInt ("a", 1);
    KB_CHECK (! settings.save());
    KB_CHECK_EQ (settings.getInt ("a", 0), 1);
}

KB_TEST (Settings, save_creates_missing_folders)
{
    kbtest::TempDir dir;
    Settings settings (dir.file ("a/b/c/s.json"));
    settings.setInt ("x", 5);
    KB_CHECK (settings.save());
    KB_CHECK (dir.file ("a/b/c/s.json").existsAsFile());
}

// --- MIDI-enheter ------------------------------------------------------------------------

KB_TEST (MidiInputManager, recognises_software_devices_to_skip)
{
    KB_CHECK (MidiInputManager::isSoftwareDevice ("Microsoft GS Wavetable Synth"));
    KB_CHECK (MidiInputManager::isSoftwareDevice ("MIDI Mapper"));
    KB_CHECK (MidiInputManager::isSoftwareDevice ("Midi Through Port-0"));
    KB_CHECK (! MidiInputManager::isSoftwareDevice ("Roland FP-30X"));
    KB_CHECK (! MidiInputManager::isSoftwareDevice ("Digital Piano USB-MIDI"));
}

KB_TEST (MidiState, tracks_keys_pedal_and_wheels)
{
    MidiState state;
    state.apply (MidiEvent::noteOn (60, 90));
    state.apply (MidiEvent::noteOn (64, 40));
    KB_CHECK (state.isDown (60) && state.isDown (64));
    KB_CHECK_EQ (state.getVelocity (64), 40);
    KB_CHECK_EQ (state.getLastNote(), 64);
    KB_CHECK_EQ (state.countKeysDown(), 2);

    state.apply (MidiEvent::noteOff (60));
    KB_CHECK (! state.isDown (60));
    KB_CHECK_EQ (state.countKeysDown(), 1);

    state.apply (MidiEvent::sustain (true));
    KB_CHECK (state.isSustainDown());
    state.apply (MidiEvent::allNotesOff());
    KB_CHECK_EQ (state.countKeysDown(), 0);
    KB_CHECK (! state.isSustainDown());

    KB_CHECK (! state.isDown (-1));
    KB_CHECK (! state.isDown (128));
    KB_CHECK_EQ (state.getVelocity (500), 0);
}
