#include "midi/MidiInputManager.h"
#include "core/Text.h"

#include "core/Logger.h"
#include "midi/MidiParser.h"

namespace kb
{
MidiInputManager::MidiInputManager (MidiHub& hubToUse, Settings& settingsToUse)
    : hub (hubToUse), settings (settingsToUse)
{
}

MidiInputManager::~MidiInputManager()
{
    shutdown();
}

void MidiInputManager::shutdown()
{
    stopTimer();
    closeDevice();
}

double MidiInputManager::nowSeconds()
{
    return juce::Time::getMillisecondCounterHiRes() * 0.001;
}

bool MidiInputManager::isSoftwareDevice (const juce::String& name)
{
    const auto lower = name.toLowerCase();
    return lower.contains ("wavetable") || lower.contains ("microsoft") || lower.contains ("midi through")
           || lower.contains ("midi mapper");
}

void MidiInputManager::start()
{
    selectedName = settings.getString ("midi.deviceName");
    selectedIdentifier = settings.getString ("midi.deviceId");

    refreshDeviceList();

    juce::String list;
    for (const auto& d : devices)
        list << "\n    - " << d.name << "  [" << d.identifier << "]";

    Logger::get().info (devices.empty() ? juce::String ("MIDI-enheter: ingen funnet")
                                        : "MIDI-enheter funnet:" + list);

    if (findSelectedDevice() == nullptr)
        autoSelect();

    if (const auto* device = findSelectedDevice())
    {
        tracker.onRetryStarted (nowSeconds());
        if (tryOpen (*device))
            tracker.onConnected (nowSeconds());
        else
            tracker.onRetryFailed (nowSeconds());
    }

    startTimer (500);
}

void MidiInputManager::refreshDeviceList()
{
    devices.clear();
    for (const auto& d : juce::MidiInput::getAvailableDevices())
        devices.push_back (d);
}

const juce::MidiDeviceInfo* MidiInputManager::findSelectedDevice() const
{
    if (selectedName.isEmpty() && selectedIdentifier.isEmpty())
        return nullptr;

    for (const auto& d : devices)
        if (selectedIdentifier.isNotEmpty() && d.identifier == selectedIdentifier)
            return &d;

    for (const auto& d : devices)
        if (selectedName.isNotEmpty() && d.name == selectedName)
            return &d;

    return nullptr;
}

bool MidiInputManager::autoSelect()
{
    for (const auto& d : devices)
    {
        if (! isSoftwareDevice (d.name))
        {
            selectedName = d.name;
            selectedIdentifier = d.identifier;
            settings.setString ("midi.deviceName", selectedName);
            settings.setString ("midi.deviceId", selectedIdentifier);
            settings.save();
            Logger::get().info ("MIDI: valgte automatisk \"" + selectedName + "\"");
            return true;
        }
    }
    return false;
}

void MidiInputManager::selectDevice (const juce::MidiDeviceInfo& device)
{
    closeDevice();
    selectedName = device.name;
    selectedIdentifier = device.identifier;
    settings.setString ("midi.deviceName", selectedName);
    settings.setString ("midi.deviceId", selectedIdentifier);
    settings.save();

    tracker.onLost (nowSeconds());
    tracker.onRetryStarted (nowSeconds());

    if (tryOpen (device))
        tracker.onConnected (nowSeconds());
    else
        tracker.onRetryFailed (nowSeconds());
}

bool MidiInputManager::tryOpen (const juce::MidiDeviceInfo& device)
{
    closeDevice();

    auto opened = juce::MidiInput::openDevice (device.identifier, this);

    if (opened == nullptr)
    {
        Logger::get().warn ("MIDI: kunne ikke åpne \""_nb + device.name
                            + "\". Et annet MIDI-program kan bruke pianoet. På Windows kan vanligvis bare ett program bruke en MIDI-enhet om gangen."_nb);
        return false;
    }

    opened->start();
    input = std::move (opened);
    Logger::get().info ("MIDI: koblet til \"" + device.name + "\"");
    return true;
}

void MidiInputManager::closeDevice()
{
    if (input != nullptr)
    {
        input->stop();
        input.reset();
    }
}

void MidiInputManager::releaseEverything (MidiHub& hubToRelease)
{
    hubToRelease.state.reset();
    hubToRelease.fifo.push (MidiEvent::allNotesOff());
}

void MidiInputManager::markLost (const juce::String& reason)
{
    closeDevice();

    releaseEverything (hub);

    if (tracker.isConnected())
        Logger::get().warn ("MIDI: " + reason);

    tracker.onLost (nowSeconds());
}

void MidiInputManager::rescan()
{
    refreshDeviceList();

    if (findSelectedDevice() == nullptr)
        autoSelect();

    if (tracker.isConnected() && findSelectedDevice() != nullptr)
        return;

    if (const auto* device = findSelectedDevice())
    {
        tracker.onLost (nowSeconds());
        tracker.onRetryStarted (nowSeconds());
        if (tryOpen (*device))
            tracker.onConnected (nowSeconds());
        else
            tracker.onRetryFailed (nowSeconds());
    }
}

void MidiInputManager::timerCallback()
{
    refreshDeviceList();
    const auto* selected = findSelectedDevice();
    const double now = nowSeconds();

    if (tracker.isConnected())
    {
        if (selected == nullptr || input == nullptr)
            markLost ("pianoet ble koblet fra (\"" + selectedName + "\"). Prøver å finne det igjen."_nb);
        return;
    }

    if (selected == nullptr)
    {
        if (selectedName.isEmpty())
            autoSelect();
        return;
    }

    if (! tracker.shouldRetry (now, retryIntervalSeconds))
        return;

    tracker.onRetryStarted (now);

    if (tryOpen (*selected))
    {
        tracker.onConnected (nowSeconds());
        Logger::get().info ("MIDI: tilkoblingen er tilbake etter "
                            + juce::String (tracker.lastOutageSeconds(), 1) + " sekunder.");
    }
    else
    {
        tracker.onRetryFailed (nowSeconds());
    }
}

void MidiInputManager::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    // Kjører på MIDI-tråden: kort og uten tung logikk.
    MidiParser parser;

    parser.feed (message.getRawData(), static_cast<std::size_t> (message.getRawDataSize()),
                 [this] (const MidiEvent& e)
                 {
                     hub.state.apply (e);

                     if (e.type == MidiEventType::NoteOn)
                         hub.counters.midiNoteOnReceived.fetch_add (1, std::memory_order_relaxed);
                     else if (e.type == MidiEventType::NoteOff)
                         hub.counters.midiNoteOffReceived.fetch_add (1, std::memory_order_relaxed);

                     hub.fifo.push (e);
                 });
}

juce::String MidiInputManager::getStatusText() const
{
    switch (tracker.state())
    {
        case ConnectionState::Connected:
            return "Tilkoblet: " + selectedName;

        case ConnectionState::Retrying:
            return "Prøver på nytt (forsøk "_nb + juce::String (tracker.attempts()) + ")";

        case ConnectionState::Lost:
            break;
    }

    if (selectedName.isEmpty())
        return devices.empty() ? "Ingen MIDI-enhet funnet" : "Velg MIDI-enhet";

    return "Mistet: " + selectedName;
}
} // namespace kb
