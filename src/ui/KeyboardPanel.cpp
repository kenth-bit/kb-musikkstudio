#include "ui/KeyboardPanel.h"

#include "core/HelpTexts.h"
#include "core/Text.h"
#include "midi/NoteMath.h"
#include "ui/HelpHub.h"
#include "ui/Layout.h"
#include "ui/Theme.h"

namespace kb
{
namespace
{
constexpr int lowestNote = 21;
constexpr int highestNote = 108;
constexpr int numWhiteKeys = 52;
constexpr float blackKeyWidthRatio = 0.62f;
constexpr float blackKeyHeightRatio = 0.62f;

bool isBlackKey (int note)
{
    switch (note % 12)
    {
        case 1: case 3: case 6: case 8: case 10: return true;
        default: return false;
    }
}

juce::Colour litColour (juce::Colour base, int velocity)
{
    const float amount = 0.45f + 0.55f * static_cast<float> (velocity) / 127.0f;
    return base.interpolatedWith (theme().keyLit, amount);
}
} // namespace

class KeyboardPanel::KeyboardView : public juce::Component
{
public:
    explicit KeyboardView (const MidiState& s) : state (s)
    {
        shown.fill (0);
    }

    /** Returnerer true hvis noe endret seg. */
    bool poll()
    {
        bool changed = false;
        for (int note = lowestNote; note <= highestNote; ++note)
        {
            const int v = state.getVelocity (note);
            if (v != shown[static_cast<size_t> (note)])
            {
                shown[static_cast<size_t> (note)] = v;
                changed = true;
            }
        }
        if (changed)
            repaint();
        return changed;
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = theme();
        const auto bounds = getLocalBounds().toFloat();
        const float whiteWidth = bounds.getWidth() / static_cast<float> (numWhiteKeys);
        const float radius = static_cast<float> (t.cornerRadius) * 0.4f;
        const float gap = 1.0f;

        int whiteIndex = 0;
        for (int note = lowestNote; note <= highestNote; ++note)
        {
            if (isBlackKey (note))
                continue;

            const auto key = juce::Rectangle<float> (bounds.getX() + whiteWidth * static_cast<float> (whiteIndex), bounds.getY(),
                                                     whiteWidth, bounds.getHeight()).reduced (gap * 0.5f, 0.0f);
            const int velocity = shown[static_cast<size_t> (note)];

            g.setColour (velocity > 0 ? litColour (t.keyWhite, velocity) : t.keyWhite);
            g.fillRoundedRectangle (key.withTrimmedTop (-radius), radius);

            if (note % 12 == 0)
            {
                g.setColour (t.keyBlack.withAlpha (0.55f));
                g.setFont (t.font (t.fontSizeSmall - 2));
                g.drawText ("C" + juce::String (note / 12 - 1), key.withTrimmedTop (key.getHeight() - 16.0f).toNearestInt(),
                            juce::Justification::centred, false);
            }
            ++whiteIndex;
        }

        whiteIndex = 0;
        for (int note = lowestNote; note <= highestNote; ++note)
        {
            if (! isBlackKey (note))
            {
                ++whiteIndex;
                continue;
            }

            const float blackWidth = whiteWidth * blackKeyWidthRatio;
            const float x = bounds.getX() + whiteWidth * static_cast<float> (whiteIndex) - blackWidth * 0.5f;
            const auto key = juce::Rectangle<float> (x, bounds.getY(), blackWidth, bounds.getHeight() * blackKeyHeightRatio);
            const int velocity = shown[static_cast<size_t> (note)];

            g.setColour (t.panelBorder);
            g.fillRoundedRectangle (key.expanded (0.5f, 0.5f).withTrimmedTop (-radius), radius);
            g.setColour (velocity > 0 ? litColour (t.keyBlack.brighter (0.5f), velocity) : t.keyBlack);
            g.fillRoundedRectangle (key.withTrimmedTop (-radius), radius);
        }

        g.setColour (t.panelBorder);
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
    }

private:
    const MidiState& state;
    std::array<int, 128> shown;
};

KeyboardPanel::KeyboardPanel (const MidiState& midiState)
    : Panel ("Tangenter"), state (midiState)
{
    setHelpId (*this, HelpId::panelKeys);

    keys = std::make_unique<KeyboardView> (state);
    setHelpId (*keys, HelpId::keyboard);
    addAndMakeVisible (*keys);

    rows[0] = std::make_unique<ValueRow> ("Siste tone"_nb, HelpId::noteInfo);
    rows[1] = std::make_unique<ValueRow> ("Velocity", HelpId::velocity);
    rows[2] = std::make_unique<ValueRow> ("Sustain-pedal", HelpId::sustainPedal);
    rows[3] = std::make_unique<ValueRow> ("Pitch bend", HelpId::pitchBend);
    rows[4] = std::make_unique<ValueRow> ("Mod-hjul", HelpId::modWheel);

    for (auto& row : rows)
        addAndMakeVisible (*row);

    timerCallback();
    startTimer (33);
}

KeyboardPanel::~KeyboardPanel() { stopTimer(); }

void KeyboardPanel::timerCallback()
{
    keys->poll();

    const int note = state.getLastNote();
    if (note >= 0)
        rows[0]->setValue (juce::String (noteName (note)) + "  " + juce::String (note) + "  "
                               + juce::String (noteToHz (note), 1) + " Hz",
                           state.isDown (note));
    else
        rows[0]->setValue ("-", false);

    rows[1]->setValue (note >= 0 ? juce::String (state.getLastVelocity()) : juce::String ("-"), false);
    rows[2]->setValue (state.isSustainDown() ? "På"_nb : juce::String ("Av"), state.isSustainDown());
    rows[3]->setValue (juce::String (state.getPitchBend()), state.getPitchBend() != 0);
    rows[4]->setValue (juce::String (state.getModWheel()), state.getModWheel() > 0);
}

void KeyboardPanel::resized()
{
    const auto& l = layout();
    auto area = getContentArea();

    auto info = area.removeFromRight (l.infoColumnWidth);
    area.removeFromRight (l.panelGap);
    keys->setBounds (area);

    const int rowHeight = info.getHeight() / static_cast<int> (rows.size());
    for (auto& row : rows)
        row->setBounds (info.removeFromTop (rowHeight));
}
} // namespace kb
