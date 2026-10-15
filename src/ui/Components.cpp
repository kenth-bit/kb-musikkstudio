#include "ui/Components.h"

#include "audio/Dsp.h"
#include "ui/HelpHub.h"
#include "ui/Layout.h"
#include "ui/Theme.h"

#include <cmath>

namespace kb
{
namespace
{
constexpr int uiRefreshMs = 33;

juce::Rectangle<float> centredSquare (const juce::Rectangle<int>& bounds, int size)
{
    return juce::Rectangle<float> (static_cast<float> (size), static_cast<float> (size))
        .withCentre (bounds.toFloat().getCentre());
}

void drawGlowDot (juce::Graphics& g, juce::Rectangle<float> dot, juce::Colour colour, bool lit)
{
    const auto& t = theme();

    if (lit)
    {
        const auto glow = dot.expanded (dot.getWidth() * 0.55f);
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.45f), glow.getCentreX(), glow.getCentreY(),
                                                 colour.withAlpha (0.0f), glow.getRight(), glow.getCentreY(), true));
        g.fillEllipse (glow);
    }

    g.setColour (lit ? colour : t.panelRaised);
    g.fillEllipse (dot);
    g.setColour (lit ? colour.brighter (0.4f) : t.panelBorder);
    g.drawEllipse (dot.reduced (0.5f), 1.0f);

    if (lit)
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse (dot.reduced (dot.getWidth() * 0.3f).translated (-dot.getWidth() * 0.08f, -dot.getHeight() * 0.1f));
    }
}
} // namespace

juce::Colour colourForStatus (StatusColour status)
{
    const auto& t = theme();
    switch (status)
    {
        case StatusColour::Green:  return t.ok;
        case StatusColour::Yellow: return t.warning;
        case StatusColour::Red:    break;
    }
    return t.danger;
}

StatusColour worstOf (StatusColour a, StatusColour b) noexcept
{
    auto rank = [] (StatusColour c) { return c == StatusColour::Red ? 2 : (c == StatusColour::Yellow ? 1 : 0); };
    return rank (a) >= rank (b) ? a : b;
}

// ---------------------------------------------------------------------------------------

Panel::Panel (juce::String panelTitle) : title (std::move (panelTitle)) {}

void Panel::setTitle (const juce::String& newTitle)
{
    title = newTitle;
    repaint();
}

juce::Rectangle<int> Panel::getContentArea() const
{
    auto area = getLocalBounds().reduced (layout().panelPadding);
    area.removeFromTop (layout().panelTitleHeight);
    return area;
}

void Panel::paint (juce::Graphics& g)
{
    const auto& t = theme();
    const auto& l = layout();
    const auto bounds = getLocalBounds().toFloat();
    const float radius = static_cast<float> (t.cornerRadius);

    g.setColour (t.panel);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (t.panelBorder);
    g.drawRoundedRectangle (bounds.reduced (static_cast<float> (t.borderThickness) * 0.5f), radius,
                            static_cast<float> (t.borderThickness));

    const auto titleArea = getLocalBounds().reduced (l.panelPadding).removeFromTop (l.panelTitleHeight);
    const float barHeight = static_cast<float> (t.fontSizeTitle);
    const float barWidth = static_cast<float> (l.grid) * 0.75f;

    g.setColour (t.accentPurple);
    g.fillRoundedRectangle (static_cast<float> (titleArea.getX()),
                            static_cast<float> (titleArea.getCentreY()) - barHeight * 0.5f,
                            barWidth, barHeight, barWidth * 0.5f);

    g.setColour (t.text);
    g.setFont (t.font (t.fontSizeTitle, true));
    g.drawText (title, titleArea.withTrimmedLeft (l.controlGap + static_cast<int> (barWidth)),
                juce::Justification::centredLeft, true);
}

// ---------------------------------------------------------------------------------------

void StatusLamp::setStatus (StatusColour newStatus)
{
    if (newStatus == status)
        return;
    status = newStatus;
    repaint();
}

void StatusLamp::paint (juce::Graphics& g)
{
    drawGlowDot (g, centredSquare (getLocalBounds(), layout().lampSize), colourForStatus (status), true);
}

// ---------------------------------------------------------------------------------------

LevelMeterView::LevelMeterView (const LevelMeter& meterToShow, int channelToShow)
    : meter (meterToShow), channel (channelToShow)
{
    setInterceptsMouseClicks (true, false);
    startTimer (uiRefreshMs);
}

LevelMeterView::~LevelMeterView() { stopTimer(); }

void LevelMeterView::timerCallback()
{
    const float peak = meter.getPeak (channel);
    const float rms = meter.getRms (channel);

    if (std::fabs (peak - shownPeak) > 0.0005f || std::fabs (rms - shownRms) > 0.0005f)
    {
        shownPeak = peak;
        shownRms = rms;
        repaint();
    }
}

void LevelMeterView::paint (juce::Graphics& g)
{
    const auto& t = theme();
    const auto bounds = getLocalBounds().toFloat();
    const float radius = static_cast<float> (layout().grid) * 0.5f;

    auto fraction = [] (float gain)
    {
        const float db = gainToDb (gain);
        return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    };

    g.setColour (t.background);
    g.fillRoundedRectangle (bounds, radius);

    const float h = bounds.getHeight();
    const float rmsTop = bounds.getBottom() - h * fraction (shownRms);
    const float peakY = bounds.getBottom() - h * fraction (shownPeak);

    juce::ColourGradient gradient (t.danger, 0.0f, bounds.getY(), t.accentCyan, 0.0f, bounds.getBottom(), false);
    gradient.addColour (0.12, t.warning);
    gradient.addColour (0.30, t.accentCyan);
    g.setGradientFill (gradient);

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (bounds.withTop (rmsTop).toNearestInt());
        g.setOpacity (0.55f);
        g.fillRoundedRectangle (bounds, radius);
    }

    g.setGradientFill (gradient);
    g.fillRect (juce::Rectangle<float> (bounds.getX(), juce::jmin (peakY, bounds.getBottom() - 2.0f), bounds.getWidth(), 2.0f));

    g.setColour (t.panelBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
}

// ---------------------------------------------------------------------------------------

ClipLamp::ClipLamp (LevelMeter& meterToWatch) : meter (meterToWatch)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimer (uiRefreshMs * 3);
}

ClipLamp::~ClipLamp() { stopTimer(); }

void ClipLamp::timerCallback()
{
    const bool clipped = meter.isClipped();
    if (clipped != shownClipped)
    {
        shownClipped = clipped;
        repaint();
    }
}

void ClipLamp::mouseDown (const juce::MouseEvent&)
{
    meter.resetClip();
    shownClipped = false;
    repaint();
}

void ClipLamp::paint (juce::Graphics& g)
{
    drawGlowDot (g, centredSquare (getLocalBounds(), layout().clipLampSize), theme().danger, shownClipped);
}

// ---------------------------------------------------------------------------------------

KnobControl::KnobControl (Parameter& parameterToControl) : parameter (parameterToControl)
{
    setHelpId (*this, parameter.info().id);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRange (0.0, 1.0, 0.0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.setDoubleClickReturnValue (true, parameter.getRange().convertTo0to1 (parameter.info().defaultValue));
    slider.setMouseDragSensitivity (180);
    slider.onValueChange = [this]
    {
        parameter.set (parameter.getRange().convertFrom0to1 (static_cast<float> (slider.getValue())));
        const auto text = parameter.formatCurrent();
        if (text != shownValueText)
        {
            shownValueText = text;
            repaint();
        }
    };
    addAndMakeVisible (slider);

    sync();
    startTimer (50);
}

KnobControl::~KnobControl() { stopTimer(); }

void KnobControl::sync()
{
    const double normalised = parameter.getRange().convertTo0to1 (parameter.get());
    if (std::fabs (slider.getValue() - normalised) > 1.0e-4)
        slider.setValue (normalised, juce::dontSendNotification);

    const auto text = parameter.formatCurrent();
    if (text != shownValueText)
    {
        shownValueText = text;
        repaint();
    }
}

void KnobControl::timerCallback()
{
    if (! slider.isMouseButtonDown())
        sync();
}

int KnobControl::preferredWidth()  { return layout().knobSize + layout().controlGap * 2; }
int KnobControl::preferredHeight() { return layout().knobSize + layout().knobLabelHeight * 2 + layout().grid * 2; }

void KnobControl::resized()
{
    const auto& l = layout();
    auto area = getLocalBounds();
    area.removeFromTop (l.knobLabelHeight + l.grid / 2);
    slider.setBounds (area.removeFromTop (l.knobSize).withSizeKeepingCentre (l.knobSize, l.knobSize));
}

void KnobControl::paint (juce::Graphics& g)
{
    const auto& t = theme();
    const auto& l = layout();
    const auto bounds = getLocalBounds();

    g.setColour (t.textDim);
    g.setFont (t.font (t.fontSizeSmall, true));
    g.drawText (parameter.info().name.toUpperCase(), bounds.withHeight (l.knobLabelHeight), juce::Justification::centred, true);

    g.setColour (t.accentCyan);
    g.setFont (t.monoFont (t.fontSizeSmall));
    g.drawText (shownValueText, bounds.withTop (bounds.getBottom() - l.knobLabelHeight), juce::Justification::centred, true);
}

// ---------------------------------------------------------------------------------------

EnvelopeGraph::EnvelopeGraph (const Parameter& attack, const Parameter& decay, const Parameter& sustain,
                              const Parameter& release)
    : attackParam (attack), decayParam (decay), sustainParam (sustain), releaseParam (release)
{
    startTimer (100);
}

EnvelopeGraph::~EnvelopeGraph() { stopTimer(); }

void EnvelopeGraph::timerCallback()
{
    const float a = attackParam.get(), d = decayParam.get(), s = sustainParam.get(), r = releaseParam.get();
    if (a != lastA || d != lastD || s != lastS || r != lastR)
    {
        lastA = a;
        lastD = d;
        lastS = s;
        lastR = r;
        repaint();
    }
}

void EnvelopeGraph::paint (juce::Graphics& g)
{
    const auto& t = theme();
    const auto& l = layout();
    const auto outer = getLocalBounds().toFloat();
    const float radius = static_cast<float> (t.cornerRadius) * 0.6f;

    g.setColour (t.background);
    g.fillRoundedRectangle (outer, radius);
    g.setColour (t.panelBorder);
    g.drawRoundedRectangle (outer.reduced (0.5f), radius, 1.0f);

    const auto area = outer.reduced (static_cast<float> (l.controlGap), static_cast<float> (l.controlGap) + 6.0f);
    if (area.getWidth() < 10.0f || area.getHeight() < 10.0f)
        return;

    const float a = juce::jmax (attackParam.get(), 0.001f);
    const float d = juce::jmax (decayParam.get(), 0.001f);
    const float s = juce::jlimit (0.0f, 1.0f, sustainParam.get());
    const float r = juce::jmax (releaseParam.get(), 0.005f);
    const float hold = juce::jmax (0.15f, 0.2f * (a + d + r));
    const float total = a + d + hold + r;

    const float x0 = area.getX();
    const float scaleX = area.getWidth() / total;
    const float bottom = area.getBottom();
    const float top = area.getY();
    auto yFor = [&] (float level) { return bottom - level * (bottom - top); };

    juce::Path path;
    path.startNewSubPath (x0, yFor (0.0f));

    constexpr int steps = 28;
    float x = x0;

    for (int i = 1; i <= steps; ++i)
        path.lineTo (x0 + (a * scaleX) * static_cast<float> (i) / steps, yFor (static_cast<float> (i) / steps));
    x += a * scaleX;
    const float xDecayStart = x;

    for (int i = 1; i <= steps; ++i)
    {
        const float f = static_cast<float> (i) / steps;
        const float level = s + (1.0f - s) * (std::exp (-5.0f * f) - std::exp (-5.0f) * f) / (1.0f - std::exp (-5.0f));
        path.lineTo (x + d * scaleX * f, yFor (level));
    }
    x += d * scaleX;
    const float xSustainStart = x;

    path.lineTo (x + hold * scaleX, yFor (s));
    x += hold * scaleX;
    const float xReleaseStart = x;

    for (int i = 1; i <= steps; ++i)
    {
        const float f = static_cast<float> (i) / steps;
        const float level = s * (std::exp (-5.0f * f) - std::exp (-5.0f) * f) / (1.0f - std::exp (-5.0f));
        path.lineTo (x + r * scaleX * f, yFor (level));
    }

    juce::Path fill (path);
    fill.lineTo (x0 + total * scaleX, bottom);
    fill.lineTo (x0, bottom);
    fill.closeSubPath();

    g.setColour (t.accentPurple.withAlpha (0.22f));
    g.fillPath (fill);

    g.setColour (t.panelBorder);
    for (const float marker : { xDecayStart, xSustainStart, xReleaseStart })
        g.drawVerticalLine (juce::roundToInt (marker), top, bottom);

    g.setColour (t.accentCyan);
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (t.textDim);
    g.setFont (t.font (t.fontSizeSmall - 1, true));
    const float labelY = outer.getBottom() - 14.0f;
    auto label = [&] (const char* text, float from, float to)
    {
        g.drawText (text, juce::Rectangle<float> (from, labelY, juce::jmax (to - from, 10.0f), 12.0f).toNearestInt(),
                    juce::Justification::centredLeft, false);
    };
    label ("A", x0 + 2.0f, xDecayStart);
    label ("D", xDecayStart + 2.0f, xSustainStart);
    label ("S", xSustainStart + 2.0f, xReleaseStart);
    label ("R", xReleaseStart + 2.0f, x0 + total * scaleX);
}

// ---------------------------------------------------------------------------------------

CaptionLabel::CaptionLabel (juce::String textToShow, bool dimmed) : text (std::move (textToShow)), dim (dimmed) {}

void CaptionLabel::setText (const juce::String& newText)
{
    if (newText == text)
        return;
    text = newText;
    repaint();
}

void CaptionLabel::setJustification (juce::Justification newJustification)
{
    justification = newJustification;
    repaint();
}

void CaptionLabel::setMaxLines (int lines)
{
    maxLines = juce::jmax (1, lines);
    repaint();
}

void CaptionLabel::setBold (bool shouldBeBold)
{
    bold = shouldBeBold;
    repaint();
}

void CaptionLabel::paint (juce::Graphics& g)
{
    const auto& t = theme();
    g.setColour (dim ? t.textDim : t.text);
    g.setFont (t.font (t.fontSizeNormal, bold));
    g.drawFittedText (text, getLocalBounds(), justification, maxLines, 1.0f);
}

// ---------------------------------------------------------------------------------------

ValueRow::ValueRow (juce::String rowName, const char* helpId) : name (std::move (rowName))
{
    setHelpId (*this, helpId);
}

void ValueRow::setValue (const juce::String& newValue, bool highlighted)
{
    if (newValue == value && highlighted == lit)
        return;
    value = newValue;
    lit = highlighted;
    repaint();
}

void ValueRow::paint (juce::Graphics& g)
{
    const auto& t = theme();
    auto area = getLocalBounds();

    g.setColour (t.textDim);
    g.setFont (t.font (t.fontSizeNormal));
    g.drawText (name, area.removeFromLeft (layout().labelWidth - layout().controlGap), juce::Justification::centredLeft, true);

    g.setColour (lit ? t.accentCyan : t.text);
    g.setFont (t.monoFont (t.fontSizeNormal));
    g.drawText (value, area, juce::Justification::centredLeft, true);

    g.setColour (t.panelBorder.withAlpha (0.6f));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}
} // namespace kb
