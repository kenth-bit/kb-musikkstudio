#include "app/StudioContext.h"

#include "core/Logger.h"

namespace kb
{
StudioContext::StudioContext (const juce::File& settingsFile)
    : settings (settingsFile),
      audio (hub, settings, parameters),
      midi (hub, settings)
{
    settings.load();

    registerBuiltInModules (modules);
    ModuleContext context { parameters, hub };

    for (auto& module : modules.createAll (context))
        audio.addModule (std::move (module));
}

StudioContext::~StudioContext()
{
    shutdown();
}

void StudioContext::start()
{
    if (running)
        return;

    running = true;
    audio.start();
    midi.start();
}

void StudioContext::shutdown()
{
    if (! running)
        return;

    running = false;
    midi.shutdown();
    audio.shutdown();
    settings.save();
}
} // namespace kb
