#pragma once

#include <juce_core/juce_core.h>

#include <utility>
#include <vector>

namespace kb
{
/** ALLE marginer, mellomrom og størrelser i programmet ligger her. Tallene er piksler.

    Slik finjusterer du:
      1. Endre tallet her og bygg på nytt, ELLER
      2. legg tallet i resources/theme.json under "layout" og trykk F5 i programmet
         (F5 virker i utviklingsbygg). Da ser du endringen med en gang uten å bygge.

    Tallene bør være delelige på 4 (rutenettet), så alt blir jevnt og ryddig.
    Ingen andre steder i koden skal ha egne tall for margin eller mellomrom. */
struct Layout
{
    // Rutenett og marger
    int grid = 4;               // grunnenhet. Alt annet bør være et multiplum av denne
    int outerMargin = 16;       // luft mellom vindukant og innholdet
    int panelGap = 12;          // mellomrom mellom panelene
    int panelPadding = 12;      // luft inni hvert panel, fra kant til innhold
    int panelTitleHeight = 28;  // høyden på tittelfeltet øverst i et panel
    int controlGap = 8;         // avstand mellom knotter, knapper og felt
    int rowHeight = 28;         // høyden på en rad med nedtrekksmeny eller knapp
    int labelWidth = 104;       // bredden på etiketten til venstre i en rad

    // Knotter og målere
    int knobSize = 64;          // diameter på en knott
    int knobLabelHeight = 16;   // høyden på teksten over og under en knott
    int faderWidth = 36;        // bredden på master-faderen
    int meterWidth = 16;        // bredden på hver målerstolpe
    int clipLampSize = 20;      // størrelsen på klipp-lampen
    int lampSize = 12;          // størrelsen på de små statuslampene
    int statusStripeHeight = 4; // høyden på statusstripen under topplinjen

    // Faste områder i vinduet
    int topBarHeight = 48;
    int helpBarHeight = 40;
    int keyboardPanelHeight = 176;
    int logPanelHeight = 168;
    int connectionPanelWidth = 360;
    int masterPanelWidth = 232;
    int infoColumnWidth = 248;  // infokolonnen til høyre for tangentbordet

    // Vindu
    int minWindowWidth = 1100;
    int minWindowHeight = 800;
    int defaultWindowWidth = 1280;
    int defaultWindowHeight = 900;

    void resetDefaults() { *this = Layout(); }

    /** Runder opp til nærmeste multiplum av rutenettet. */
    int snap (int value) const noexcept
    {
        const int g = grid > 0 ? grid : 1;
        return ((value + g / 2) / g) * g;
    }

    /** Navn og felter, brukt av theme.json. Samme navn som i filen. */
    std::vector<std::pair<const char*, int*>> fields()
    {
        return {
            { "grid", &grid }, { "outerMargin", &outerMargin }, { "panelGap", &panelGap },
            { "panelPadding", &panelPadding }, { "panelTitleHeight", &panelTitleHeight },
            { "controlGap", &controlGap }, { "rowHeight", &rowHeight }, { "labelWidth", &labelWidth },
            { "knobSize", &knobSize }, { "knobLabelHeight", &knobLabelHeight }, { "faderWidth", &faderWidth },
            { "meterWidth", &meterWidth }, { "clipLampSize", &clipLampSize }, { "lampSize", &lampSize },
            { "statusStripeHeight", &statusStripeHeight }, { "topBarHeight", &topBarHeight },
            { "helpBarHeight", &helpBarHeight }, { "keyboardPanelHeight", &keyboardPanelHeight },
            { "logPanelHeight", &logPanelHeight },
            { "connectionPanelWidth", &connectionPanelWidth }, { "masterPanelWidth", &masterPanelWidth },
            { "infoColumnWidth", &infoColumnWidth }, { "minWindowWidth", &minWindowWidth },
            { "minWindowHeight", &minWindowHeight }, { "defaultWindowWidth", &defaultWindowWidth },
            { "defaultWindowHeight", &defaultWindowHeight }
        };
    }
};

/** Den delte layouten som hele programmet bruker. */
Layout& layout();
} // namespace kb
