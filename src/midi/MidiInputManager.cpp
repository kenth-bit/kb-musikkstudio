#include "midi/MidiInputManager.h"
#include "core/Text.h"

#include "core/Logger.h"
#include "midi/PackedMidi.h"

#if JUCE_WINDOWS
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <mmsystem.h>
#endif

namespace kb
{
struct NativeMidi
{
#if JUCE_WINDOWS
    HMIDIIN handle = nullptr;
    std::atomic<bool> accept { false };
    std::atomic<bool> closed { false };
    MidiHub* hub = nullptr;
#endif
};

#if JUCE_WINDOWS
void CALLBACK winMidiProc (HMIDIIN, UINT message, DWORD_PTR instance, DWORD_PTR data, DWORD_PTR)
{
    auto* port = reinterpret_cast<NativeMidi*> (instance);

    if (port == nullptr || port->hub == nullptr || ! port->accept.load (std::memory_order_acquire))
        return;

    if (message == MIM_DATA)
        applyPackedMidi (static_cast<std::uint32_t> (data), *port->hub);
    else if (message == MIM_CLOSE)
        port->closed.store (true, std::memory_order_release);
}

int windowsMidiIndex (const juce::String& name)
{
    const UINT count = midiInGetNumDevs();

    for (UINT i = 0; i < count; ++i)
    {
        MIDIINCAPSW caps {};

        if (midiInGetDevCapsW (i, &caps, sizeof (caps)) != MMSYSERR_NOERROR)
            continue;

        if (juce::String (caps.szPname) == name)
            return static_cast<int> (i);
    }

    return -1;
}
#endif

MidiInputManager::MidiInputManager (MidiHub& hubToUse, Settings& settingsToUse)
    : hub (hubToUse), settings (settingsToUse), native (std::make_unique<NativeMidi>())
{
#if JUCE_WINDOWS
    native->hub = &hub;
#endif
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

bool MidiInputManager::deviceIsOpen() const noexcept
{
#if JUCE_WINDOWS
    return native != nullptr && native->handle != nullptr;
#else
    return input != nullptr;
#endif
}

void MidiInputManager::refreshDeviceList()
{
    devices.clear();

#if JUCE_WINDOWS
    const UINT count = midiInGetNumDevs();

    for (UINT i = 0; i < count; ++i)
    {
        MIDIINCAPSW caps {};

        if (midiInGetDevCapsW (i, &caps, sizeof (caps)) != MMSYSERR_NOERROR)
            continue;

        juce::MidiDeviceInfo info;
        info.name = juce::String (caps.szPname);
        info.identifier = "winmm:" + juce::String (static_cast<int> (i));
        devices.push_back (info);
    }
#else
    for (const auto& d : juce::MidiInput::getAvailableDevices())
        devices.push_back (d);
#endif
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

#if JUCE_WINDOWS
    int index = windowsMidiIndex (device.name);

    if (index < 0 && device.identifier.startsWith ("winmm:"))
        index = device.identifier.fromFirstOccurrenceOf ("winmm:", false, false).getIntValue();

    if (index < 0)
    {
        Logger::get().warn ("MIDI: kunne ikke åpne \""_nb + device.name + "\".");
        return false;
    }

    native->closed.store (false);
    native->accept.store (true, std::memory_order_release);

    HMIDIIN handle = nullptr;
    const auto err = midiInOpen (&handle,
                                  static_cast<UINT> (index),
                                  reinterpret_cast<DWORD_PTR> (&winMidiProc),
                                  reinterpret_cast<DWORD_PTR> (native.get()),
                                  CALLBACK_FUNCTION);

    if (err != MMSYSERR_NOERROR || midiInStart (handle) != MMSYSERR_NOERROR)
    {
        native->accept.store (false, std::memory_order_release);

        if (handle != nullptr)
            midiInClose (handle);

        Logger::get().warn ("MIDI: kunne ikke åpne \""_nb + device.name
                            + "\". Et annet MIDI-program kan bruke pianoet. På Windows kan vanligvis bare ett program bruke en MIDI-enhet om gangen."_nb);
        return false;
    }

    native->handle = handle;
    Logger::get().info ("MIDI: koblet til \"" + device.name + "\"");
    return true;
#else
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
#endif
}

void MidiInputManager::closeDevice()
{
#if JUCE_WINDOWS
    if (native != nullptr)
        native->accept.store (false, std::memory_order_release);

    if (native != nullptr && native->handle != nullptr)
    {
        const auto handle = native->handle;
        native->handle = nullptr;
        midiInStop (handle);
        midiInReset (handle);
        midiInClose (handle);
    }
#endif

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
#if JUCE_WINDOWS
        const bool unplugged = native != nullptr && native->closed.load (std::memory_order_acquire);
#else
        const bool unplugged = false;
#endif
        if (selected == nullptr || ! deviceIsOpen() || unplugged)
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
    const auto* raw = message.getRawData();
    const int size = message.getRawDataSize();

    if (raw == nullptr || size <= 0 || size > 3 || static_cast<std::uint8_t> (raw[0]) < 0x80)
        return;

    std::uint32_t packed = static_cast<std::uint8_t> (raw[0]);

    if (size > 1)
        packed |= static_cast<std::uint32_t> (static_cast<std::uint8_t> (raw[1])) << 8;

    if (size > 2)
        packed |= static_cast<std::uint32_t> (static_cast<std::uint8_t> (raw[2])) << 16;

    applyPackedMidi (packed, hub);
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
