#include "ui/MasterPanel.h"

#include "audio/Dsp.h"
#include "core/HelpTexts.h"
#include "ui/HelpHub.h"
#include "ui/Layout.h"
#include "ui/Theme.h"

#include <cmath>

namespace kb
{
class MasterPanel::MeterScale : public juce::Component
{
public:
    void paint (juce::Graphics& g) override
    {
        const auto& t = theme();
        g.setColour (t.textDim);
        g.setFont (t.monoFont (t.fontSizeSmall - 1));

        const auto bounds = getLocalBounds();
        const int labelHeight = theme().fontSizeSmall;
        int lastY = -1000;

        for (const int db : { 0, -6, -12, -24, -36, -48 })
        {
            const float fraction = juce::jlimit (0.0f, 1.0f, (static_cast<float> (db) + 60.0f) / 60.0f);
            const int y = bounds.getBottom() - juce::roundToInt (fraction * static_cast<float> (bounds.getHeight()));
            if (std::abs (y - lastY) < labelHeight + 1)
                continue;

            lastY = y;
            const auto textArea = juce::Rectangle<int> (bounds.getX(), y - labelHeight / 2, bounds.getWidth(), labelHeight)
                                      .constrainedWithin (bounds);
            g.drawText (juce::String (db), textArea, juce::Justification::centredRight, false);
        }
    }
};

class MasterPanel::FaderValue : public juce::Component
{
public:
    explicit FaderValue (const Parameter& p) : parameter (p) {}

    void paint (juce::Graphics& g) override
    {
        const auto& t = theme();
        g.setColour (t.accentCyan);
        g.setFont (t.monoFont (t.fontSizeNormal));
        g.drawText (parameter.formatCurrent(), getLocalBounds(), juce::Justification::centred, true);
    }

private:
    const Parameter& parameter;
};

MasterPanel::MasterPanel (AudioEngine& audioEngine, ParameterRegistry& parameters)
    : Panel ("Master"),
      audio (audioEngine),
      masterParam (parameters.require (HelpId::master)),
      meterLeft (audioEngine.getMeter(), 0),
      meterRight (audioEngine.getMeter(), 1),
      clipLamp (audioEngine.getMeter())
{
    setHelpId (*this, HelpId::panelMaster);

    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    fader.setRange (0.0, 1.0, 0.0);
    fader.setDoubleClickReturnValue (true, masterParam.getRange().convertTo0to1 (masterParam.info().defaultValue));
    fader.onValueChange = [this]
    {
        masterParam.set (masterParam.getRange().convertFrom0to1 (static_cast<float> (fader.getValue())));
        if (faderValue != nullptr)
            faderValue->repaint();
    };
    setHelpId (fader, HelpId::master);
    addAndMakeVisible (fader);

    faderValue = std::make_unique<FaderValue> (masterParam);
    setHelpId (*faderValue, HelpId::master);
    addAndMakeVisible (*faderValue);

    scale = std::make_unique<MeterScale>();
    scale->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (*scale);

    setHelpId (meterLeft, HelpId::meter);
    setHelpId (meterRight, HelpId::meter);
    addAndMakeVisible (meterLeft);
    addAndMakeVisible (meterRight);

    setHelpId (clipLamp, HelpId::clip);
    setHelpId (clipLabel, HelpId::clip);
    addAndMakeVisible (clipLamp);
    addAndMakeVisible (clipLabel);

    testToneButton.setClickingTogglesState (true);
    testToneButton.onClick = [this] { audio.setTestToneEnabled (testToneButton.getToggleState()); };
    setHelpId (testToneButton, HelpId::testTone);
    addAndMakeVisible (testToneButton);

    panicButton.onClick = [this]
    {
        audio.panic();
        testToneButton.setToggleState (false, juce::dontSendNotification);
    };
    setHelpId (panicButton, HelpId::panic);
    addAndMakeVisible (panicButton);

    applyButtonColours();
    syncFader();
    startTimer (100);
}

MasterPanel::~MasterPanel() { stopTimer(); }

void MasterPanel::applyButtonColours()
{
    const auto& t = theme();
    panicButton.setColour (juce::TextButton::buttonColourId, t.danger.darker (0.55f));
    panicButton.setColour (juce::TextButton::buttonOnColourId, t.danger);
}

void MasterPanel::lookAndFeelChanged() { applyButtonColours(); }

void MasterPanel::syncFader()
{
    const double normalised = masterParam.getRange().convertTo0to1 (masterParam.get());
    if (std::fabs (fader.getValue() - normalised) > 1.0e-4)
        fader.setValue (normalised, juce::dontSendNotification);
    if (faderValue != nullptr)
        faderValue->repaint();
}

void MasterPanel::timerCallback()
{
    if (! fader.isMouseButtonDown())
        syncFader();

    if (testToneButton.getToggleState() != audio.isTestToneEnabled())
        testToneButton.setToggleState (audio.isTestToneEnabled(), juce::dontSendNotification);
}

void MasterPanel::resized()
{
    const auto& l = layout();
    auto area = getContentArea();

    auto clipRow = area.removeFromTop (l.clipLampSize);
    clipLamp.setBounds (clipRow.removeFromLeft (l.clipLampSize));
    clipRow.removeFromLeft (l.controlGap);
    clipLabel.setBounds (clipRow);
    area.removeFromTop (l.controlGap);

    auto bottom = area.removeFromBottom (l.rowHeight * 2 + l.controlGap);
    area.removeFromBottom (l.controlGap);
    testToneButton.setBounds (bottom.removeFromTop (l.rowHeight));
    bottom.removeFromTop (l.controlGap);
    panicButton.setBounds (bottom);

    auto valueRow = area.removeFromBottom (l.rowHeight);
    area.removeFromBottom (l.controlGap / 2);

    const int scaleWidth = l.faderWidth - l.grid;
    const int groupWidth = l.faderWidth + l.panelGap + scaleWidth + l.controlGap / 2 * 2 + l.meterWidth * 2;
    auto group = area.withSizeKeepingCentre (juce::jmin (groupWidth, area.getWidth()), area.getHeight());

    auto faderArea = group.removeFromLeft (l.faderWidth);
    fader.setBounds (faderArea);
    faderValue->setBounds (valueRow);

    group.removeFromLeft (l.panelGap);
    scale->setBounds (group.removeFromLeft (scaleWidth));
    group.removeFromLeft (l.controlGap / 2);
    meterLeft.setBounds (group.removeFromLeft (l.meterWidth));
    group.removeFromLeft (l.controlGap / 2);
    meterRight.setBounds (group.removeFromLeft (l.meterWidth));
}
} // namespace kb
