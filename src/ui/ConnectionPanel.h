#pragma once

#include "audio/AudioEngine.h"
#include "core/Text.h"
#include "midi/MidiInputManager.h"
#include "ui/Components.h"

namespace kb
{
/** Velg piano (MIDI) og lydkort, se om begge er koblet til, og koble til på nytt. */
class ConnectionPanel : public Panel,
                        private juce::Timer
{
public:
    ConnectionPanel (AudioEngine& audioEngine, MidiInputManager& midiManager);
    ~ConnectionPanel() override;

    void resized() override;

    /** Oppdaterer lister og status med en gang. */
    void refresh();

private:
    void timerCallback() override;

    static void fillCombo (juce::ComboBox& box, const juce::StringArray& items, const juce::String& selected);

    AudioEngine& audio;
    MidiInputManager& midi;

    CaptionLabel midiSection { "PIANO (MIDI)" };
    CaptionLabel midiLabel { "MIDI-enhet", false };
    juce::ComboBox midiCombo;
    StatusLamp midiLamp;
    CaptionLabel midiStatus { {}, false };
    juce::TextButton rescanButton { "Skann på nytt"_nb };

    CaptionLabel audioSection { "LYD (SCARLETT)" };
    CaptionLabel driverLabel { "Lyddriver", false };
    juce::ComboBox driverCombo;
    CaptionLabel outputLabel { "Lydutgang", false };
    juce::ComboBox outputCombo;
    CaptionLabel rateLabel { "Sample rate", false };
    juce::ComboBox rateCombo;
    CaptionLabel bufferLabel { "Buffer", false };
    juce::ComboBox bufferCombo;
    CaptionLabel latencyLabel { "Forsinkelse", false };
    CaptionLabel latencyValue { {}, false };
    StatusLamp audioLamp;
    CaptionLabel audioStatus { {}, false };
    juce::TextButton reconnectButton { "Koble til på nytt"_nb };

    bool updating = false;
};
} // namespace kb
