#include "ui/MainComponent.h"

#include "core/HelpTexts.h"
#include "core/Logger.h"
#include "core/Text.h"
#include "ui/Layout.h"
#include "ui/Theme.h"
#include "ui/ThemeLoader.h"

namespace kb
{
class MainComponent::TopBar : public juce::Component,
                              private juce::Timer
{
public:
    explicit TopBar (StudioContext& studio) : context (studio)
    {
        setHelpId (midiLamp, HelpId::midiStatus);
        setHelpId (midiText, HelpId::midiStatus);
        setHelpId (audioLamp, HelpId::audioStatus);
        setHelpId (audioText, HelpId::audioStatus);
        setHelpId (cpuText, HelpId::cpu);

        cpuText.setJustification (juce::Justification::centredRight);
        for (auto* c : std::initializer_list<juce::Component*> { &midiLamp, &midiText, &audioLamp, &audioText, &cpuText })
            addAndMakeVisible (c);

        timerCallback();
        startTimer (250);
    }

    ~TopBar() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        const auto& t = theme();
        const auto& l = layout();
        g.fillAll (t.topBar);

        auto area = getLocalBounds().reduced (l.outerMargin, 0);

        const auto brand = juce::String ("KB");
        const auto name = juce::String (" Musikkstudio");
        const auto brandFont = t.font (t.fontSizeHeading, true);
        const auto nameFont = t.font (t.fontSizeHeading, false);

        juce::GlyphArrangement arrangement;
        arrangement.addLineOfText (brandFont, brand, 0.0f, 0.0f);
        const int brandWidth = juce::roundToInt (arrangement.getBoundingBox (0, -1, true).getWidth()) + 1;

        g.setFont (brandFont);
        g.setColour (t.accentMagenta);
        g.drawText (brand, area.withWidth (brandWidth), juce::Justification::centredLeft, false);

        g.setFont (nameFont);
        g.setColour (t.text);
        g.drawText (name, area.withTrimmedLeft (brandWidth).withWidth (l.labelWidth * 2), juce::Justification::centredLeft, false);

        g.setColour (t.panelBorder);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
    }

    void resized() override
    {
        const auto& l = layout();
        auto area = getLocalBounds().reduced (l.outerMargin, 0);

        cpuText.setBounds (area.removeFromRight (l.labelWidth));
        area.removeFromRight (l.panelGap);

        audioText.setBounds (area.removeFromRight (l.labelWidth - l.panelGap * 2));
        audioLamp.setBounds (area.removeFromRight (l.lampSize + l.controlGap).withSizeKeepingCentre (l.lampSize, l.lampSize));
        area.removeFromRight (l.panelGap);

        midiText.setBounds (area.removeFromRight (l.labelWidth - l.panelGap * 2));
        midiLamp.setBounds (area.removeFromRight (l.lampSize + l.controlGap).withSizeKeepingCentre (l.lampSize, l.lampSize));
    }

    StatusColour getWorstStatus() const
    {
        return worstOf (context.midi.getTracker().colour(), context.audio.getTracker().colour());
    }

private:
    void timerCallback() override
    {
        midiLamp.setStatus (context.midi.getTracker().colour());
        audioLamp.setStatus (context.audio.getTracker().colour());
        cpuText.setText ("CPU " + juce::String (juce::roundToInt (context.audio.getCpuUsage() * 100.0f)) + " %");
    }

    StudioContext& context;
    StatusLamp midiLamp, audioLamp;
    CaptionLabel midiText { "MIDI", false };
    CaptionLabel audioText { "Lyd"_nb, false };
    CaptionLabel cpuText { "CPU", true };
};

class MainComponent::StatusStripe : public juce::Component,
                                    private juce::Timer
{
public:
    explicit StatusStripe (StudioContext& studio) : context (studio)
    {
        timerCallback();
        startTimer (250);
    }

    ~StatusStripe() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        const auto colour = colourForStatus (status);
        g.setColour (colour);
        g.fillRect (getLocalBounds());
    }

private:
    void timerCallback() override
    {
        const auto worst = worstOf (context.midi.getTracker().colour(), context.audio.getTracker().colour());
        if (worst != status)
        {
            status = worst;
            repaint();
        }
    }

    StudioContext& context;
    StatusColour status = StatusColour::Red;
};

class MainComponent::HelpBar : public juce::Component,
                               private juce::ChangeListener
{
public:
    HelpBar() { HelpHub::get().addChangeListener (this); }
    ~HelpBar() override { HelpHub::get().removeChangeListener (this); }

    void paint (juce::Graphics& g) override
    {
        const auto& t = theme();
        const auto& l = layout();

        g.fillAll (t.topBar);
        g.setColour (t.panelBorder);
        g.drawHorizontalLine (0, 0.0f, static_cast<float> (getWidth()));

        auto area = getLocalBounds().reduced (l.outerMargin, l.controlGap);
        const auto badge = area.removeFromLeft (l.clipLampSize).withSizeKeepingCentre (l.clipLampSize, l.clipLampSize).toFloat();
        g.setColour (t.accentPurple.withAlpha (0.25f));
        g.fillEllipse (badge);
        g.setColour (t.accentPurple);
        g.drawEllipse (badge.reduced (0.5f), 1.0f);
        g.setColour (t.text);
        g.setFont (t.font (t.fontSizeSmall, true));
        g.drawText ("?", badge.toNearestInt(), juce::Justification::centred, false);

        area.removeFromLeft (l.panelGap);
        g.setColour (HelpHub::get().getText() == HelpHub::defaultText() ? t.textDim : t.text);
        g.setFont (t.font (t.fontSizeNormal));
        g.drawFittedText (HelpHub::get().getText(), area, juce::Justification::centredLeft, 2, 1.0f);
    }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
};

MainComponent::MainComponent (StudioContext& studioContext) : context (studioContext)
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);

    topBar = std::make_unique<TopBar> (context);
    stripe = std::make_unique<StatusStripe> (context);
    connectionPanel = std::make_unique<ConnectionPanel> (context.audio, context.midi);
    masterPanel = std::make_unique<MasterPanel> (context.audio, context.parameters);
    keyboardPanel = std::make_unique<KeyboardPanel> (context.hub.state);
    logPanel = std::make_unique<LogPanel> (context.hub);
    helpBar = std::make_unique<HelpBar>();

    for (const auto& module : context.audio.getModules())
        modulePanels.push_back (module->createPanel());

    addAndMakeVisible (*topBar);
    addAndMakeVisible (*stripe);
    addAndMakeVisible (*connectionPanel);
    for (auto& panel : modulePanels)
        addAndMakeVisible (*panel);
    addAndMakeVisible (*masterPanel);
    addAndMakeVisible (*keyboardPanel);
    addAndMakeVisible (*logPanel);
    addAndMakeVisible (*helpBar);

    helpTracker = std::make_unique<HelpTracker> (*this, context.parameters);

    setSize (layout().defaultWindowWidth, layout().defaultWindowHeight);
}

MainComponent::~MainComponent()
{
    helpTracker.reset();
    setLookAndFeel (nullptr);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme().background);
}

void MainComponent::resized()
{
    const auto& l = layout();
    auto area = getLocalBounds();

    topBar->setBounds (area.removeFromTop (l.topBarHeight));
    stripe->setBounds (area.removeFromTop (l.statusStripeHeight));
    helpBar->setBounds (area.removeFromBottom (l.helpBarHeight));

    area.reduce (l.outerMargin, l.outerMargin);

    keyboardPanel->setBounds (area.removeFromBottom (l.keyboardPanelHeight));
    area.removeFromBottom (l.panelGap);

    connectionPanel->setBounds (area.removeFromLeft (l.connectionPanelWidth));
    area.removeFromLeft (l.panelGap);

    logPanel->setBounds (area.removeFromBottom (l.logPanelHeight));
    area.removeFromBottom (l.panelGap);

    juce::FlexBox modulesColumn;
    modulesColumn.flexDirection = juce::FlexBox::Direction::column;
    for (size_t i = 0; i < modulePanels.size(); ++i)
    {
        const float gap = i + 1 < modulePanels.size() ? static_cast<float> (l.panelGap) : 0.0f;
        modulesColumn.items.add (juce::FlexItem (*modulePanels[i]).withFlex (1.0f)
                                     .withMargin (juce::FlexItem::Margin (0, 0, gap, 0)));
    }

    juce::FlexBox row;
    row.flexDirection = juce::FlexBox::Direction::row;
    row.items.add (juce::FlexItem (modulesColumn).withFlex (1.0f)
                       .withMargin (juce::FlexItem::Margin (0, static_cast<float> (l.panelGap), 0, 0)));
    row.items.add (juce::FlexItem (*masterPanel).withWidth (static_cast<float> (l.masterPanelWidth)));
    row.performLayout (area);
}

void MainComponent::reloadTheme()
{
    const bool usedFile = ThemeLoader::reload();
    Logger::get().info (usedFile ? "Tema og layout er lastet på nytt (F5)."_nb
                                 : "Fant ingen theme.json. Bruker standardverdier (F5)."_nb);
    lookAndFeel.refresh();
    sendLookAndFeelChange();
    resized();
    repaint();

    if (onThemeReloaded != nullptr)
        onThemeReloaded();
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
#if JUCE_DEBUG
    if (key == juce::KeyPress::F5Key)
    {
        reloadTheme();
        return true;
    }
#endif

    juce::ignoreUnused (key);
    return false;
}
} // namespace kb
