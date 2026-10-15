#include "ui/HelpHub.h"
#include "core/Text.h"

namespace kb
{
HelpHub& HelpHub::get()
{
    static HelpHub instance;
    return instance;
}

juce::String HelpHub::defaultText()
{
    return "Pek på en knapp, knott eller et felt, så står forklaringen her."_nb;
}

void HelpHub::setText (const juce::String& newText)
{
    if (newText == text)
        return;

    text = newText;
    sendChangeMessage();
}

void setHelpId (juce::Component& component, const juce::String& id)
{
    component.getProperties().set ("helpId", id);
}

HelpTracker::HelpTracker (juce::Component& rootComponent, const ParameterRegistry& registryToUse)
    : root (rootComponent), registry (registryToUse)
{
    root.addMouseListener (this, true);
}

HelpTracker::~HelpTracker()
{
    root.removeMouseListener (this);
}

void HelpTracker::update (const juce::MouseEvent& event)
{
    for (auto* c = event.originalComponent; c != nullptr; c = c->getParentComponent())
    {
        if (c->getProperties().contains ("helpId"))
        {
            const auto text = registry.getHelp (c->getProperties()["helpId"].toString());
            HelpHub::get().setText (text.isNotEmpty() ? text : HelpHub::defaultText());
            return;
        }
    }

    HelpHub::get().setText (HelpHub::defaultText());
}

void HelpTracker::mouseMove (const juce::MouseEvent& event)  { update (event); }
void HelpTracker::mouseEnter (const juce::MouseEvent& event) { update (event); }

void HelpTracker::mouseExit (const juce::MouseEvent& event)
{
    if (event.originalComponent == &root)
        HelpHub::get().setText (HelpHub::defaultText());
}
} // namespace kb
