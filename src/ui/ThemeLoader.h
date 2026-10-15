#pragma once

#include "ui/Layout.h"
#include "ui/Theme.h"

namespace kb
{
/** Leser resources/theme.json og overstyrer verdiene i Theme og Layout.

    Filformat (alt er valgfritt):
    {
      "layout":  { "outerMargin": 24, "panelGap": 16 },
      "theme":   { "cornerRadius": 10 },
      "colours": { "accentCyan": "#3fe0ff" }
    }

    Mangler filen, eller er den ødelagt, brukes standardverdiene. Det er aldri en krasj-grunn. */
class ThemeLoader
{
public:
    /** Bruker innholdet i en JSON-tekst. Returnerer false hvis teksten er ugyldig
        (da er ingenting endret). Advarsler (ukjente navn, dårlige verdier) legges i `warnings`. */
    static bool applyJson (const juce::String& json, Theme& themeToChange, Layout& layoutToChange,
                           juce::StringArray& warnings);

    /** Finner theme.json: først i kildemappen (så F5 ser endringer med en gang), så ved siden
        av programmet, så i brukermappen. Returnerer en tom File hvis ingen finnes. */
    static juce::File findThemeFile();

    /** Nullstiller til standardverdier og leser filen på nytt. Returnerer true hvis en fil ble brukt. */
    static bool reload (Theme& themeToChange, Layout& layoutToChange);
    static bool reload() { return reload (theme(), layout()); }
};
} // namespace kb
