#pragma once

#include "core/ConnectionTracker.h"
#include "core/Settings.h"
#include "midi/MidiHub.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>

#include <memory>
#include <vector>

namespace kb
{
struct NativeMidi;


/** Lytter på MIDI-enheten (pianoet), legger hendelser i køen og passer på at tilkoblingen
    kommer tilbake av seg selv hvis USB-kabelen trekkes ut og settes inn.
    Alle offentlige funksjoner skal kalles fra meldingstråden (UI-tråden). */
class MidiInputManager : private juce::MidiInputCallback,
                         private juce::Timer
{
public:
    MidiInputManager (MidiHub& hub, Settings& settings);
    ~MidiInputManager() override;

    /** Skanner enheter, velger automatisk og begynner å passe på tilkoblingen. */
    void start();

    /** Stopper overvåkingen og lukker MIDI-enheten. Trygt å kalle flere ganger. */
    void shutdown();

    /** "Skann på nytt": oppdaterer listen og gjør ett forsøk umiddelbart. */
    void rescan();

    const std::vector<juce::MidiDeviceInfo>& getDevices() const noexcept { return devices; }
    juce::String getSelectedName() const { return selectedName; }
    void selectDevice (const juce::MidiDeviceInfo& device);

    const ConnectionTracker& getTracker() const noexcept { return tracker; }
    juce::String getStatusText() const;

    /** Det som skjer når pianoet forsvinner: alle tangenter og pedalen regnes som sluppet, og en
        "alle toner av"-melding sendes til lydtråden. Offentlig så testene kan bruke den. */
    static void releaseEverything (MidiHub& hub);

    /** Brukes av tester og automatisk valg: er dette en programvareenhet vi skal hoppe over? */
    static bool isSoftwareDevice (const juce::String& name);

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void timerCallback() override;

    void refreshDeviceList();
    const juce::MidiDeviceInfo* findSelectedDevice() const;
    bool autoSelect();
    bool tryOpen (const juce::MidiDeviceInfo& device);
    void closeDevice();
    void markLost (const juce::String& reason);
    static double nowSeconds();
    bool deviceIsOpen() const noexcept;

    MidiHub& hub;
    Settings& settings;

    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<NativeMidi> native;
    std::vector<juce::MidiDeviceInfo> devices;
    juce::String selectedName;
    juce::String selectedIdentifier;
    ConnectionTracker tracker;

    static constexpr double retryIntervalSeconds = 2.0;
};
} // namespace kb
