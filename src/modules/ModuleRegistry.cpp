#include "modules/ModuleRegistry.h"

#include "synth/SynthModule.h"

namespace kb
{
void ModuleRegistry::registerModule (const juce::String& id, Factory factory)
{
    entries.push_back ({ id, std::move (factory) });
}

std::vector<std::unique_ptr<StudioModule>> ModuleRegistry::createAll (ModuleContext& context) const
{
    std::vector<std::unique_ptr<StudioModule>> result;
    for (const auto& entry : entries)
        result.push_back (entry.factory (context));
    return result;
}

juce::StringArray ModuleRegistry::getIds() const
{
    juce::StringArray ids;
    for (const auto& entry : entries)
        ids.add (entry.id);
    return ids;
}

void registerBuiltInModules (ModuleRegistry& registry)
{
    registry.registerModule ("synth", [] (ModuleContext& context)
    {
        return std::make_unique<SynthModule> (context.parameters, &context.hub.counters, &context.hub.state);
    });
}
} // namespace kb
