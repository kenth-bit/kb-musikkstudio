#include "ui/LogPanel.h"

#include "core/HelpTexts.h"
#include "core/Logger.h"
#include "core/Text.h"
#include "ui/HelpHub.h"
#include "ui/Layout.h"
#include "ui/Theme.h"

namespace kb
{
LogPanel::LogPanel (MidiHub& midiHub) : Panel ("Logg"), hub (midiHub)
{
    setHelpId (*this, HelpId::panelLog);

    editor.setMultiLine (true, false);
    editor.setReadOnly (true);
    editor.setCaretVisible (false);
    editor.setScrollbarsShown (true);
    editor.setPopupMenuEnabled (false);
    setHelpId (editor, HelpId::log);
    addAndMakeVisible (editor);

    openFolderButton.onClick = []
    {
        const auto file = Logger::get().getLogFile();
        if (file.existsAsFile())
            file.revealToUser();
        else if (file != juce::File())
            file.getParentDirectory().startAsProcess();
    };
    setHelpId (openFolderButton, HelpId::log);
    addAndMakeVisible (openFolderButton);

    setHelpId (debugTitle, HelpId::logDebug);
    addAndMakeVisible (debugTitle);

    rows[0] = std::make_unique<ValueRow> ("Tangent ned"_nb, HelpId::logDebug);
    rows[1] = std::make_unique<ValueRow> ("Tangent opp"_nb, HelpId::logDebug);
    rows[2] = std::make_unique<ValueRow> ("Tapt i køen"_nb, HelpId::logDebug);
    rows[3] = std::make_unique<ValueRow> ("Nødstopp"_nb, HelpId::logDebug);
    rows[4] = std::make_unique<ValueRow> ("Aktive stemmer", HelpId::logDebug);

    for (auto& row : rows)
        addAndMakeVisible (*row);

    applyEditorLook();
    refresh();
    startTimer (250);
}

LogPanel::~LogPanel() { stopTimer(); }

void LogPanel::applyEditorLook()
{
    editor.setFont (theme().monoFont (theme().fontSizeSmall));
    rebuildText();
}

void LogPanel::lookAndFeelChanged()
{
    applyEditorLook();
}

void LogPanel::rebuildText()
{
    const auto& t = theme();
    const auto lines = Logger::get().getRecentLines (200);

    editor.clear();
    for (const auto& line : lines)
    {
        auto colour = t.text;
        if (line.contains ("  FEIL  "))
            colour = t.danger;
        else if (line.contains ("  ADVAR  "))
            colour = t.warning;
        else if (line.contains ("  DEBUG  "))
            colour = t.textDim;

        editor.setColour (juce::TextEditor::textColourId, colour);
        editor.moveCaretToEnd();
        editor.insertTextAtCaret (line + "\n");
    }
    editor.setColour (juce::TextEditor::textColourId, t.text);
    editor.moveCaretToEnd();
}

void LogPanel::refresh()
{
    const int revision = Logger::get().getRevision();
    if (revision != shownRevision)
    {
        shownRevision = revision;
        rebuildText();
    }

    const auto& c = hub.counters;
    const auto onReceived = c.midiNoteOnReceived.load();
    const auto onDone = c.noteOnProcessed.load();
    const auto offReceived = c.midiNoteOffReceived.load();
    const auto offDone = c.noteOffProcessed.load();
    const auto dropped = hub.fifo.getDroppedCount();
    const auto watchdog = c.watchdogReleases.load();

    rows[0]->setValue (juce::String (onReceived) + " mott. / " + juce::String (onDone) + " beh.", onReceived != onDone);
    rows[1]->setValue (juce::String (offReceived) + " mott. / " + juce::String (offDone) + " beh.", offReceived != offDone);
    rows[2]->setValue (juce::String (dropped), dropped > 0);
    rows[3]->setValue (juce::String (watchdog), watchdog > 0);
    rows[4]->setValue (juce::String (c.activeVoices.load()), false);
}

void LogPanel::timerCallback() { refresh(); }

void LogPanel::resized()
{
    const auto& l = layout();

    auto titleRow = getLocalBounds().reduced (l.panelPadding).removeFromTop (l.panelTitleHeight);
    openFolderButton.setBounds (titleRow.removeFromRight (l.labelWidth + l.panelGap * 3)
                                    .withSizeKeepingCentre (l.labelWidth + l.panelGap * 3, l.rowHeight - l.grid));

    auto area = getContentArea();
    auto info = area.removeFromRight (l.infoColumnWidth);
    area.removeFromRight (l.panelGap);
    editor.setBounds (area);

    const int rowHeight = info.getHeight() / static_cast<int> (rows.size() + 1);
    debugTitle.setBounds (info.removeFromTop (rowHeight));
    for (auto& row : rows)
        row->setBounds (info.removeFromTop (rowHeight));
}
} // namespace kb
