#pragma once

#include "core/ParameterRegistry.h"

#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kb
{
/** Holder teksten som vises i hjelpelinjen nederst. */
class HelpHub : public juce::ChangeBroadcaster
{
public:
    static HelpHub& get();

    void setText (const juce::String& newText);
    juce::String getText() const { return text; }

    static juce::String defaultText();

private:
    juce::String text = defaultText();
};

/** Knytter en kontroll til en hjelpetekst (id-en slås opp i ParameterRegistry). */
void setHelpId (juce::Component& component, const juce::String& id);

/** Lytter på musen i hele vinduet og viser hjelpeteksten til kontrollen musen peker på.
    Den innerste kontrollen med en hjelp-id vinner, ellers brukes panelets tekst. */
class HelpTracker : public juce::MouseListener
{
public:
    HelpTracker (juce::Component& root, const ParameterRegistry& registry);
    ~HelpTracker() override;

    void mouseMove (const juce::MouseEvent& event) override;
    void mouseEnter (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;

private:
    void update (const juce::MouseEvent& event);

    juce::Component& root;
    const ParameterRegistry& registry;
};
} // namespace kb
