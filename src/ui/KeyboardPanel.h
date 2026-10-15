#pragma once

#include "midi/MidiState.h"
#include "ui/Components.h"

#include <array>
#include <memory>

namespace kb
{
/** Pianoet på skjermen (88 tangenter, A0 til C8) og info om det som spilles. */
class KeyboardPanel : public Panel,
                      private juce::Timer
{
public:
    explicit KeyboardPanel (const MidiState& midiState);
    ~KeyboardPanel() override;

    void resized() override;

private:
    class KeyboardView;

    void timerCallback() override;

    const MidiState& state;
    std::unique_ptr<KeyboardView> keys;
    std::array<std::unique_ptr<ValueRow>, 5> rows;
};
} // namespace kb
