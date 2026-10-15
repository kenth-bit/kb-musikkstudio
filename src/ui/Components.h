#pragma once

#include "audio/LevelMeter.h"
#include "core/ConnectionTracker.h"
#include "core/ParameterRegistry.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace kb
{
/** Farge for en statuslampe. Brukes av alle lamper og av statusstripen. */
juce::Colour colourForStatus (StatusColour status);

/** Den verste av to statusfarger (rød er verst). */
StatusColour worstOf (StatusColour a, StatusColour b) noexcept;

/** Grunnflate for alle paneler: avrundet boks med kant og tittel. Alle mål kommer fra Layout og Theme. */
class Panel : public juce::Component
{
public:
    explicit Panel (juce::String panelTitle);

    void setTitle (const juce::String& newTitle);
    juce::String getTitle() const { return title; }

    void paint (juce::Graphics&) override;

    /** Området inni panelet under tittelen, med indre margin. */
    juce::Rectangle<int> getContentArea() const;

private:
    juce::String title;
};

/** Rund lampe som lyser grønn, gul eller rød. */
class StatusLamp : public juce::Component
{
public:
    StatusLamp() { setInterceptsMouseClicks (true, false); }

    void setStatus (StatusColour newStatus);
    StatusColour getStatus() const noexcept { return status; }

    void paint (juce::Graphics&) override;

private:
    StatusColour status = StatusColour::Red;
};

/** Stolpemåler for én eller to kanaler (RMS som flate, peak som strek). Oppdaterer seg selv. */
class LevelMeterView : public juce::Component,
                       private juce::Timer
{
public:
    LevelMeterView (const LevelMeter& meterToShow, int channelToShow);
    ~LevelMeterView() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    const LevelMeter& meter;
    int channel;
    float shownPeak = 0.0f;
    float shownRms = 0.0f;
};

/** Klipp-lampe. Lyser rødt når lyden har gått over 0 dB. Klikk nullstiller. */
class ClipLamp : public juce::Component,
                 private juce::Timer
{
public:
    explicit ClipLamp (LevelMeter& meterToWatch);
    ~ClipLamp() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    LevelMeter& meter;
    bool shownClipped = false;
};

/** Knott bundet til en parameter: navn over, verdi under, dobbeltklikk for å nullstille.
    Hjelpeteksten hentes fra parameter-id-en. */
class KnobControl : public juce::Component,
                    private juce::Timer
{
public:
    explicit KnobControl (Parameter& parameterToControl);
    ~KnobControl() override;

    /** Henter verdien fra parameteren (for eksempel etter at en startlyd er valgt). */
    void sync();

    Parameter& getParameter() noexcept { return parameter; }
    juce::Slider& getSlider() noexcept { return slider; }

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Bredden og høyden en knott trenger, regnet fra Layout. */
    static int preferredWidth();
    static int preferredHeight();

private:
    void timerCallback() override;

    Parameter& parameter;
    juce::Slider slider;
    juce::String shownValueText;
};

/** Tegner ADSR-kurven ut fra parametrene. Tegnes på nytt når verdiene endres. */
class EnvelopeGraph : public juce::Component,
                      private juce::Timer
{
public:
    EnvelopeGraph (const Parameter& attack, const Parameter& decay, const Parameter& sustain, const Parameter& release);
    ~EnvelopeGraph() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    const Parameter& attackParam;
    const Parameter& decayParam;
    const Parameter& sustainParam;
    const Parameter& releaseParam;
    float lastA = -1.0f, lastD = -1.0f, lastS = -1.0f, lastR = -1.0f;
};

/** Liten tekst som er en del av et panel og som har en hjelpetekst. */
class CaptionLabel : public juce::Component
{
public:
    explicit CaptionLabel (juce::String textToShow, bool dimmed = true);

    void setText (const juce::String& newText);
    void setJustification (juce::Justification newJustification);
    void setMaxLines (int lines);
    void setBold (bool shouldBeBold);
    void paint (juce::Graphics&) override;

private:
    juce::String text;
    bool dim;
    bool bold = false;
    int maxLines = 1;
    juce::Justification justification { juce::Justification::centredLeft };
};

/** Rad med et navn til venstre og en verdi til høyre, med en tynn strek under. */
class ValueRow : public juce::Component
{
public:
    ValueRow (juce::String rowName, const char* helpId);

    void setValue (const juce::String& newValue, bool highlighted = false);
    void paint (juce::Graphics&) override;

private:
    juce::String name;
    juce::String value;
    bool lit = false;
};
} // namespace kb
