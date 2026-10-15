#pragma once

#include "app/StudioContext.h"
#include "ui/Components.h"
#include "ui/ConnectionPanel.h"
#include "ui/HelpHub.h"
#include "ui/KBLookAndFeel.h"
#include "ui/KeyboardPanel.h"
#include "ui/LogPanel.h"
#include "ui/MasterPanel.h"

#include <functional>
#include <memory>
#include <vector>

namespace kb
{
/** Hovedinnholdet i vinduet: topplinje, statusstripe, paneler og hjelpelinje. */
class MainComponent : public juce::Component
{
public:
    explicit MainComponent (StudioContext& studioContext);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    /** Leser resources/theme.json på nytt og tegner alt om igjen. Brukes av F5 i utviklingsbygg. */
    void reloadTheme();

    /** Kalles etter at tema eller layout er lastet på nytt (for eksempel for å oppdatere minstestørrelsen). */
    std::function<void()> onThemeReloaded;

private:
    class TopBar;
    class StatusStripe;
    class HelpBar;

    StudioContext& context;
    KBLookAndFeel lookAndFeel;

    std::unique_ptr<TopBar> topBar;
    std::unique_ptr<StatusStripe> stripe;
    std::unique_ptr<ConnectionPanel> connectionPanel;
    std::vector<std::unique_ptr<juce::Component>> modulePanels;
    std::unique_ptr<MasterPanel> masterPanel;
    std::unique_ptr<KeyboardPanel> keyboardPanel;
    std::unique_ptr<LogPanel> logPanel;
    std::unique_ptr<HelpBar> helpBar;
    std::unique_ptr<HelpTracker> helpTracker;
};
} // namespace kb
