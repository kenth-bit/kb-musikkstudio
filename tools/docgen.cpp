#include "core/HelpTexts.h"
#include "core/ParameterRegistry.h"
#include "core/Text.h"
#include "midi/MidiHub.h"
#include "modules/ModuleRegistry.h"

#include <juce_events/juce_events.h>

#include <map>

#include <algorithm>
#include <iostream>

namespace
{
using namespace kb;

struct Entry
{
    juce::String name;
    juce::String help;
    juce::String range;
};

juce::String describeRange (const Parameter& p)
{
    const auto& info = p.info();
    return "Område: "_nb + p.format (info.minValue) + " til " + p.format (info.maxValue)
           + ". Standard: " + p.format (info.defaultValue) + ". Dobbeltklikk på knotten for å nullstille."_nb;
}
} // namespace

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI initialiser;

    if (argc < 2)
    {
        std::cerr << "Bruk: kb_docgen <utfil.md>\n";
        return 2;
    }

    ParameterRegistry registry;
    registerCoreParameters (registry);

    MidiHub hub;
    ModuleRegistry modules;
    registerBuiltInModules (modules);
    ModuleContext context { registry, hub };
    auto created = modules.createAll (context);

    juce::StringArray groupOrder { "Tilkobling", "Synth", "Master", "Tangenter", "Logg", "Topplinje", "Vinduet" };
    std::map<juce::String, std::vector<Entry>> groups;

    for (const auto* p : registry.allParameters())
    {
        groups[p->info().group].push_back ({ p->info().name, p->info().help, describeRange (*p) });
        if (! groupOrder.contains (p->info().group))
            groupOrder.add (p->info().group);
    }

    for (const auto& c : registry.allControls())
    {
        groups[c.group].push_back ({ c.name, c.help, {} });
        if (! groupOrder.contains (c.group))
            groupOrder.add (c.group);
    }

    juce::String text;
    text << "# Knappeguide\n\n"
         << "Her står det hva hver knapp, knott og lampe i KB Musikkstudio gjør. "_nb
         << "Samme tekster vises i hjelpelinjen nederst i programmet når du peker på en kontroll.\n\n"_nb
         << "> Denne filen lages automatisk fra teksten i koden. Ikke rediger den for hånd. "_nb
         << "Lag den på nytt med `cmake --build build --target docs`.\n\n"_nb;

    for (const auto& group : groupOrder)
    {
        const auto it = groups.find (group);
        if (it == groups.end())
            continue;

        text << "## " << group << "\n\n";
        for (const auto& entry : it->second)
        {
            text << "### " << entry.name << "\n\n" << entry.help << "\n\n";
            if (entry.range.isNotEmpty())
                text << "*" << entry.range << "*\n\n";
        }
    }

    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    out.getParentDirectory().createDirectory();

    if (! out.replaceWithText (text))
    {
        std::cerr << "Kunne ikke skrive " << out.getFullPathName() << "\n";
        return 1;
    }

    std::cout << "Skrev " << out.getFullPathName() << "\n";
    return 0;
}
