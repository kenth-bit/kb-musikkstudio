#pragma once

#include "midi/MidiState.h"
#include "synth/SynthModule.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace kb
{
/** Lager panelet til synth-modulen: startlyder, ADSR-kurve og seks knotter. */
std::unique_ptr<juce::Component> createSynthPanel (SynthModule& module, ParameterRegistry& registry, DebugCounters* counters);
} // namespace kb
