#include "ui/ConnectionPanel.h"
#include "core/Text.h"

#include "core/HelpTexts.h"
#include "ui/HelpHub.h"
#include "ui/Layout.h"

namespace kb
{
namespace
{
constexpr int rateIds[] = { 44100, 48000 };
constexpr int bufferSizes[] = { 64, 128, 256, 512 };
} // namespace

ConnectionPanel::ConnectionPanel (AudioEngine& audioEngine, MidiInputManager& midiManager)
    : Panel ("Tilkobling"), audio (audioEngine), midi (midiManager)
{
    setHelpId (*this, HelpId::panelConnection);

    for (auto* c : std::initializer_list<juce::Component*> { &midiSection, &audioSection })
        addAndMakeVisible (c);

    auto attach = [this] (juce::Component& c, const char* helpId)
    {
        setHelpId (c, helpId);
        addAndMakeVisible (c);
    };

    attach (midiLabel, HelpId::midiDevice);
    attach (midiCombo, HelpId::midiDevice);
    attach (midiLamp, HelpId::midiStatus);
    attach (midiStatus, HelpId::midiStatus);
    attach (rescanButton, HelpId::midiRescan);

    attach (driverLabel, HelpId::audioDriver);
    attach (driverCombo, HelpId::audioDriver);
    attach (outputLabel, HelpId::audioOutput);
    attach (outputCombo, HelpId::audioOutput);
    attach (rateLabel, HelpId::audioSampleRate);
    attach (rateCombo, HelpId::audioSampleRate);
    attach (bufferLabel, HelpId::audioBuffer);
    attach (bufferCombo, HelpId::audioBuffer);
    attach (latencyLabel, HelpId::audioLatency);
    attach (latencyValue, HelpId::audioLatency);
    attach (audioLamp, HelpId::audioStatus);
    attach (audioStatus, HelpId::audioStatus);
    attach (reconnectButton, HelpId::audioReconnect);

    midiStatus.setMaxLines (1);
    audioStatus.setMaxLines (2);
    midiSection.setBold (true);
    audioSection.setBold (true);

    midiCombo.setTextWhenNothingSelected ("Velg MIDI-enhet");
    midiCombo.setTextWhenNoChoicesAvailable ("Ingen MIDI-enhet funnet");
    driverCombo.setTextWhenNothingSelected ("Velg lyddriver");
    driverCombo.setTextWhenNoChoicesAvailable ("Ingen lyddriver funnet");
    outputCombo.setTextWhenNothingSelected ("Velg lydutgang");
    outputCombo.setTextWhenNoChoicesAvailable ("Ingen lydenhet funnet");

    for (const int rate : rateIds)
        rateCombo.addItem (juce::String (rate / 1000.0, 1) + " kHz", rate);
    for (const int size : bufferSizes)
        bufferCombo.addItem (juce::String (size) + " prøver"_nb, size);

    midiCombo.onChange = [this]
    {
        if (updating)
            return;
        const auto index = midiCombo.getSelectedItemIndex();
        const auto& devices = midi.getDevices();
        if (index >= 0 && index < static_cast<int> (devices.size()))
            midi.selectDevice (devices[static_cast<size_t> (index)]);
        refresh();
    };

    driverCombo.onChange = [this]
    {
        if (updating)
            return;
        audio.selectDriver (driverCombo.getText());
        refresh();
    };

    outputCombo.onChange = [this]
    {
        if (updating)
            return;
        audio.selectOutput (outputCombo.getText());
        refresh();
    };

    rateCombo.onChange = [this]
    {
        if (updating)
            return;
        audio.selectSampleRate (static_cast<double> (rateCombo.getSelectedId()));
        refresh();
    };

    bufferCombo.onChange = [this]
    {
        if (updating)
            return;
        audio.selectBufferSize (bufferCombo.getSelectedId());
        refresh();
    };

    rescanButton.onClick = [this]
    {
        midi.rescan();
        refresh();
    };

    reconnectButton.onClick = [this]
    {
        audio.reconnectNow();
        refresh();
    };

    refresh();
    startTimer (250);
}

ConnectionPanel::~ConnectionPanel() { stopTimer(); }

void ConnectionPanel::fillCombo (juce::ComboBox& box, const juce::StringArray& items, const juce::String& selected)
{
    if (box.isPopupActive())
        return;

    juce::StringArray current;
    for (int i = 0; i < box.getNumItems(); ++i)
        current.add (box.getItemText (i));

    if (current != items)
    {
        box.clear (juce::dontSendNotification);
        int id = 1;
        for (const auto& item : items)
            box.addItem (item, id++);
    }

    const int index = items.indexOf (selected);
    if (index >= 0)
        box.setSelectedItemIndex (index, juce::dontSendNotification);
    else
        box.setSelectedId (0, juce::dontSendNotification);
}

void ConnectionPanel::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);

    juce::StringArray midiNames;
    for (const auto& d : midi.getDevices())
        midiNames.add (d.name);
    fillCombo (midiCombo, midiNames, midi.getSelectedName());

    midiLamp.setStatus (midi.getTracker().colour());
    midiStatus.setText (midi.getStatusText());

    fillCombo (driverCombo, audio.getDriverNames(), audio.getCurrentDriver());
    fillCombo (outputCombo, audio.getOutputDeviceNames(), audio.getCurrentOutput());

    if (! rateCombo.isPopupActive())
        rateCombo.setSelectedId (juce::roundToInt (audio.getCurrentSampleRate()), juce::dontSendNotification);
    if (! bufferCombo.isPopupActive())
        bufferCombo.setSelectedId (audio.getCurrentBufferSize(), juce::dontSendNotification);

    latencyValue.setText (juce::String (audio.getBufferLatencyMs(), 1) + " ms");
    audioLamp.setStatus (audio.getTracker().colour());
    audioStatus.setText (audio.getStatusText());
}

void ConnectionPanel::timerCallback() { refresh(); }

void ConnectionPanel::resized()
{
    const auto& l = layout();
    auto area = getContentArea();

    auto nextRow = [&] (int height)
    {
        auto row = area.removeFromTop (height);
        area.removeFromTop (l.controlGap);
        return row;
    };

    auto labelled = [&] (CaptionLabel& label, juce::Component& field)
    {
        auto row = nextRow (l.rowHeight);
        label.setBounds (row.removeFromLeft (l.labelWidth));
        field.setBounds (row);
    };

    midiSection.setBounds (nextRow (l.knobLabelHeight));
    labelled (midiLabel, midiCombo);

    {
        auto row = nextRow (l.rowHeight);
        midiLamp.setBounds (row.removeFromLeft (l.lampSize + l.controlGap).withTrimmedRight (l.controlGap));
        midiStatus.setBounds (row);
    }

    rescanButton.setBounds (nextRow (l.rowHeight));

    area.removeFromTop (l.controlGap);

    audioSection.setBounds (nextRow (l.knobLabelHeight));
    labelled (driverLabel, driverCombo);
    labelled (outputLabel, outputCombo);
    labelled (rateLabel, rateCombo);
    labelled (bufferLabel, bufferCombo);
    labelled (latencyLabel, latencyValue);

    {
        auto row = nextRow (l.rowHeight * 3 / 2);
        audioLamp.setBounds (row.removeFromLeft (l.lampSize + l.controlGap).withTrimmedRight (l.controlGap));
        audioStatus.setBounds (row);
    }

    reconnectButton.setBounds (nextRow (l.rowHeight));
}
} // namespace kb
