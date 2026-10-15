#pragma once

#include "TestFramework.h"
#include "TestUtil.h"

#include "midi/MidiState.h"
#include "synth/SynthModule.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace kbtest
{
/** Samler synth, tangenttilstand og tellere, og oppfører seg som MIDI-tråden:
    en fysisk handling oppdaterer MidiState med en gang, og hendelsen leveres til synthen
    (eller går tapt, hvis testen ber om det). */
class SynthRig
{
public:
    explicit SynthRig (double sampleRate = 48000.0, int maxBlock = 1024, bool useKeyState = true)
        : synth (registry, &counters, useKeyState ? &state : nullptr),
          rate (sampleRate),
          left (static_cast<size_t> (maxBlock), 0.0f),
          right (static_cast<size_t> (maxBlock), 0.0f)
    {
        synth.prepare (sampleRate, maxBlock);
    }

    void setParam (const char* id, float value) { registry.require (id).set (value); }

    kb::MidiEvent press (int note, int velocity, int offset = 0)
    {
        auto e = kb::MidiEvent::noteOn (note, velocity, 0, offset);
        state.apply (e);
        counters.midiNoteOnReceived.fetch_add (1);
        return e;
    }

    kb::MidiEvent release (int note, int offset = 0)
    {
        auto e = kb::MidiEvent::noteOff (note, 0, offset);
        state.apply (e);
        counters.midiNoteOffReceived.fetch_add (1);
        return e;
    }

    kb::MidiEvent pedal (bool down, int offset = 0)
    {
        auto e = kb::MidiEvent::sustain (down, offset);
        state.apply (e);
        return e;
    }

    /** Kjører én lydblokk. Sjekker at utgangen er endelig og begrenset. Returnerer toppnivået. */
    float block (const std::vector<kb::MidiEvent>& events, int numSamples)
    {
        synth.process (left.data(), right.data(), numSamples, events.data(), static_cast<int> (events.size()));
        ++blocksRun;

        if (! allFinite (left, numSamples) || ! allFinite (right, numSamples))
            ++nonFiniteBlocks;

        const float peak = std::max (peakOf (left, numSamples), peakOf (right, numSamples));
        maxPeak = std::max (maxPeak, peak);
        return peak;
    }

    float block (int numSamples) { return block ({}, numSamples); }

    /** Kjører stillhet (ingen hendelser) i så mange sekunder. Returnerer toppnivået i siste blokk. */
    float run (double seconds, int blockSize = 256)
    {
        float lastPeak = 0.0f;
        const int blocks = std::max (1, static_cast<int> (std::ceil (seconds * rate / blockSize)));
        for (int i = 0; i < blocks; ++i)
            lastPeak = block (blockSize);
        return lastPeak;
    }

    kb::ParameterRegistry registry;
    kb::MidiState state;
    kb::DebugCounters counters;
    kb::SynthModule synth;

    double rate;
    std::vector<float> left, right;
    int blocksRun = 0;
    int nonFiniteBlocks = 0;
    float maxPeak = 0.0f;
};
} // namespace kbtest
