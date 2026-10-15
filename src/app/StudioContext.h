#pragma once

#include "audio/AudioEngine.h"
#include "core/HelpTexts.h"
#include "core/ParameterRegistry.h"
#include "core/Settings.h"
#include "midi/MidiHub.h"
#include "midi/MidiInputManager.h"
#include "modules/ModuleRegistry.h"

namespace kb
{
/** Eier alt som lever så lenge programmet kjører, i riktig rekkefølge:
    innstillinger, parametre, MIDI-hub, moduler, lydmotor og MIDI-inngang.
    Rekkefølgen på medlemmene bestemmer hvordan alt bygges opp og ryddes bort igjen. */
class StudioContext
{
public:
    explicit StudioContext (const juce::File& settingsFile);
    ~StudioContext();

    /** Starter lydmotor og MIDI. Kalles etter at vinduet er klart. */
    void start();
    void shutdown();

    struct CoreParameters : ParameterRegistry
    {
        CoreParameters() { registerCoreParameters (*this); }
    };

    Settings settings;
    CoreParameters parameters;
    MidiHub hub;
    ModuleRegistry modules;
    AudioEngine audio;
    MidiInputManager midi;

private:
    bool running = false;
};
} // namespace kb
