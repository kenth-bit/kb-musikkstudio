#pragma once

#include "midi/MidiHub.h"
#include "modules/StudioModule.h"

#include <functional>
#include <memory>
#include <vector>

namespace kb
{
/** Det en modul trenger for å bli laget. */
struct ModuleContext
{
    ParameterRegistry& parameters;
    MidiHub& hub;
};

/** Her registreres alle moduler. Å legge til en ny modul betyr: skriv klassen,
    og legg én linje i registerBuiltInModules(). Kjernen trenger ikke endres. */
class ModuleRegistry
{
public:
    using Factory = std::function<std::unique_ptr<StudioModule> (ModuleContext&)>;

    void registerModule (const juce::String& id, Factory factory);
    std::vector<std::unique_ptr<StudioModule>> createAll (ModuleContext& context) const;
    juce::StringArray getIds() const;

private:
    struct Entry
    {
        juce::String id;
        Factory factory;
    };
    std::vector<Entry> entries;
};

void registerBuiltInModules (ModuleRegistry& registry);
} // namespace kb
