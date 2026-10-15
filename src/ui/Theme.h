#pragma once

#include <juce_graphics/juce_graphics.h>

#include <utility>
#include <vector>

namespace kb
{
/** Farger, fonter og former. Mørkt kontrollrom med 80-talls neon (fiolett, cyan, magenta)
    brukt kun som aksent. Endre her, eller overstyr i resources/theme.json og trykk F5. */
struct Theme
{
    // Flater
    juce::Colour background   { 0xff0c0b12 };
    juce::Colour panel        { 0xff15141f };
    juce::Colour panelRaised  { 0xff1d1b2a };
    juce::Colour panelBorder  { 0xff2a2840 };
    juce::Colour topBar       { 0xff100f19 };

    // Tekst
    juce::Colour text         { 0xffe9e7f5 };
    juce::Colour textDim      { 0xff8c88a6 };

    // Aksenter (80-talls neon)
    juce::Colour accentPurple  { 0xff9b5cff };
    juce::Colour accentCyan    { 0xff3fe0ff };
    juce::Colour accentMagenta { 0xffff4fd8 };

    // Status
    juce::Colour ok      { 0xff3ddc84 };
    juce::Colour warning { 0xffffc23d };
    juce::Colour danger  { 0xffff4d5e };

    // Tangenter
    juce::Colour keyWhite { 0xffe8e6f2 };
    juce::Colour keyBlack { 0xff1a1826 };
    juce::Colour keyLit   { 0xff3fe0ff };

    // Størrelser
    int fontSizeSmall = 12;
    int fontSizeNormal = 14;
    int fontSizeTitle = 15;
    int fontSizeHeading = 22;
    int cornerRadius = 8;
    int borderThickness = 1;

    void resetDefaults() { *this = Theme(); }

    std::vector<std::pair<const char*, juce::Colour*>> colourFields()
    {
        return {
            { "background", &background }, { "panel", &panel }, { "panelRaised", &panelRaised },
            { "panelBorder", &panelBorder }, { "topBar", &topBar }, { "text", &text }, { "textDim", &textDim },
            { "accentPurple", &accentPurple }, { "accentCyan", &accentCyan }, { "accentMagenta", &accentMagenta },
            { "ok", &ok }, { "warning", &warning }, { "danger", &danger },
            { "keyWhite", &keyWhite }, { "keyBlack", &keyBlack }, { "keyLit", &keyLit }
        };
    }

    std::vector<std::pair<const char*, int*>> sizeFields()
    {
        return {
            { "fontSizeSmall", &fontSizeSmall }, { "fontSizeNormal", &fontSizeNormal },
            { "fontSizeTitle", &fontSizeTitle }, { "fontSizeHeading", &fontSizeHeading },
            { "cornerRadius", &cornerRadius }, { "borderThickness", &borderThickness }
        };
    }

    juce::Font font (int size, bool bold = false) const;
    juce::Font monoFont (int size) const;
};

/** Det delte temaet som hele programmet bruker. */
Theme& theme();
} // namespace kb
