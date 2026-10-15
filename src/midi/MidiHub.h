#pragma once

#include "midi/MidiEventFifo.h"
#include "midi/MidiState.h"

namespace kb
{
/** Alt som deles mellom MIDI-tråden, lydmotoren og UI på ett sted. */
struct MidiHub
{
    MidiEventFifo fifo;
    MidiState state;
    DebugCounters counters;
};
} // namespace kb
