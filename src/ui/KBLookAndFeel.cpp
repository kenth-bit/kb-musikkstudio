#include "ui/KBLookAndFeel.h"

#include "ui/Layout.h"
#include "ui/Theme.h"

namespace kb
{
KBLookAndFeel::KBLookAndFeel()
{
    refresh();
}

void KBLookAndFeel::refresh()
{
    const auto& t = theme();

    setColour (juce::ResizableWindow::backgroundColourId, t.background);
    setColour (juce::Label::textColourId, t.text);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::ComboBox::backgroundColourId, t.panelRaised);
    setColour (juce::ComboBox::textColourId, t.text);
    setColour (juce::ComboBox::outlineColourId, t.panelBorder);
    setColour (juce::ComboBox::arrowColourId, t.accentCyan);
    setColour (juce::ComboBox::focusedOutlineColourId, t.accentCyan);

    setColour (juce::PopupMenu::backgroundColourId, t.panelRaised);
    setColour (juce::PopupMenu::textColourId, t.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, t.accentPurple.withAlpha (0.55f));
    setColour (juce::PopupMenu::highlightedTextColourId, t.text);

    setColour (juce::TextButton::buttonColourId, t.panelRaised);
    setColour (juce::TextButton::buttonOnColourId, t.accentPurple);
    setColour (juce::TextButton::textColourOffId, t.text);
    setColour (juce::TextButton::textColourOnId, t.text);

    setColour (juce::Slider::rotarySliderFillColourId, t.accentCyan);
    setColour (juce::Slider::rotarySliderOutlineColourId, t.panelBorder);
    setColour (juce::Slider::thumbColourId, t.text);
    setColour (juce::Slider::trackColourId, t.accentCyan);
    setColour (juce::Slider::backgroundColourId, t.panelBorder);

    setColour (juce::TextEditor::backgroundColourId, t.background);
    setColour (juce::TextEditor::textColourId, t.text);
    setColour (juce::TextEditor::outlineColourId, t.panelBorder);
    setColour (juce::TextEditor::focusedOutlineColourId, t.panelBorder);
    setColour (juce::TextEditor::highlightColourId, t.accentPurple.withAlpha (0.5f));
    setColour (juce::ScrollBar::thumbColourId, t.panelBorder.brighter (0.3f));
}

juce::Font KBLookAndFeel::getComboBoxFont (juce::ComboBox&)           { return theme().font (theme().fontSizeNormal); }
juce::Font KBLookAndFeel::getLabelFont (juce::Label& label)           { return label.getFont(); }
juce::Font KBLookAndFeel::getTextButtonFont (juce::TextButton&, int)  { return theme().font (theme().fontSizeNormal, true); }
juce::Font KBLookAndFeel::getPopupMenuFont()                          { return theme().font (theme().fontSizeNormal); }

void KBLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                      float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto& t = theme();
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                static_cast<float> (width), static_cast<float> (height)).reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float arcWidth = juce::jmax (3.0f, radius * 0.14f);
    const float arcRadius = radius - arcWidth * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (t.panelBorder);
    g.strokePath (track, juce::PathStrokeType (arcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (slider.isEnabled())
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (value, juce::PathStrokeType (arcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float bodyRadius = arcRadius - arcWidth - 3.0f;
    if (bodyRadius > 4.0f)
    {
        g.setGradientFill (juce::ColourGradient (t.panelRaised.brighter (0.25f), centre.x, centre.y - bodyRadius,
                                                 t.panelRaised.darker (0.3f), centre.x, centre.y + bodyRadius, false));
        g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
        g.setColour (t.panelBorder);
        g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.0f);

        juce::Path pointer;
        const float pointerLength = bodyRadius * 0.7f;
        pointer.addRoundedRectangle (-1.5f, -bodyRadius + 3.0f, 3.0f, pointerLength * 0.6f, 1.5f);
        g.setColour (t.text);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }
}

void KBLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                      float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const auto& t = theme();

    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }

    const float grooveWidth = 6.0f;
    const float centreX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
    const juce::Rectangle<float> groove (centreX - grooveWidth * 0.5f, static_cast<float> (y) + 4.0f,
                                         grooveWidth, static_cast<float> (height) - 8.0f);

    g.setColour (t.background);
    g.fillRoundedRectangle (groove, 3.0f);
    g.setColour (t.panelBorder);
    g.drawRoundedRectangle (groove, 3.0f, 1.0f);

    juce::Rectangle<float> filled = groove.withTop (sliderPos);
    g.setColour (slider.findColour (juce::Slider::trackColourId).withAlpha (0.8f));
    g.fillRoundedRectangle (filled, 3.0f);

    const float thumbWidth = static_cast<float> (width) - 4.0f;
    const juce::Rectangle<float> thumb (centreX - thumbWidth * 0.5f, sliderPos - 7.0f, thumbWidth, 14.0f);
    g.setGradientFill (juce::ColourGradient (t.panelRaised.brighter (0.4f), thumb.getX(), thumb.getY(),
                                             t.panelRaised.darker (0.1f), thumb.getX(), thumb.getBottom(), false));
    g.fillRoundedRectangle (thumb, 3.0f);
    g.setColour (t.accentCyan);
    g.drawRoundedRectangle (thumb, 3.0f, 1.0f);
    g.setColour (t.text);
    g.fillRect (thumb.getX() + 5.0f, thumb.getCentreY() - 0.5f, thumb.getWidth() - 10.0f, 1.0f);
}

void KBLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown, int, int, int, int,
                                  juce::ComboBox& box)
{
    const auto& t = theme();
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
    const float radius = static_cast<float> (t.cornerRadius) * 0.6f;

    g.setColour (isButtonDown ? t.panelRaised.brighter (0.1f) : box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (box.hasKeyboardFocus (true) ? t.accentCyan : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);

    juce::Path arrow;
    const float cx = static_cast<float> (width) - 14.0f;
    const float cy = static_cast<float> (height) * 0.5f;
    arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.fillPath (arrow);
}

void KBLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                          bool highlighted, bool down)
{
    const auto& t = theme();
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const float radius = static_cast<float> (t.cornerRadius) * 0.6f;

    auto base = backgroundColour;
    if (! button.isEnabled())
        base = base.withAlpha (0.4f);
    else if (down)
        base = base.darker (0.2f);
    else if (highlighted)
        base = base.brighter (0.15f);

    g.setColour (base);
    g.fillRoundedRectangle (bounds, radius);

    const bool toggledOn = button.getToggleState();
    g.setColour (toggledOn ? t.accentCyan : t.panelBorder);
    g.drawRoundedRectangle (bounds, radius, toggledOn ? 1.5f : 1.0f);
}
} // namespace kb
