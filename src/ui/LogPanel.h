#pragma once

#include "core/Text.h"
#include "midi/MidiHub.h"
#include "ui/Components.h"

#include <array>
#include <memory>

namespace kb
{
/** De siste loggmeldingene, og tellere for feilsøking av hengende toner. */
class LogPanel : public Panel,
                 private juce::Timer
{
public:
    explicit LogPanel (MidiHub& midiHub);
    ~LogPanel() override;

    void resized() override;
    void lookAndFeelChanged() override;

    /** Leser loggen og tellerne på nytt med en gang. */
    void refresh();

private:
    void timerCallback() override;
    void rebuildText();
    void applyEditorLook();

    MidiHub& hub;
    juce::TextEditor editor;
    juce::TextButton openFolderButton { "Åpne loggmappen"_nb };
    CaptionLabel debugTitle { "FEILSØKING"_nb };
    std::array<std::unique_ptr<ValueRow>, 5> rows;
    int shownRevision = -1;
};
} // namespace kb
