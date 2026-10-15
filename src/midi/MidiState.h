#pragma once

#include "midi/MidiEvent.h"

#include <atomic>
#include <cstdint>

namespace kb
{
/** Tilstanden til pianoet akkurat nå: hvilke tangenter som er nede, pedal og hjul.
    Skrives fra MIDI-tråden og leses av UI (tangentbordet) og av synthens sikkerhetsnett.
    Alt er atomics, så ingen låser trengs. */
class MidiState
{
public:
    MidiState() { reset(); }

    bool isDown (int note) const noexcept
    {
        return note >= 0 && note < 128 && velocities[note].load (std::memory_order_relaxed) > 0;
    }

    int getVelocity (int note) const noexcept
    {
        return note >= 0 && note < 128 ? velocities[note].load (std::memory_order_relaxed) : 0;
    }

    bool isSustainDown() const noexcept { return sustain.load (std::memory_order_relaxed); }
    int getModWheel() const noexcept { return modWheel.load (std::memory_order_relaxed); }
    int getPitchBend() const noexcept { return pitchBend.load (std::memory_order_relaxed); }
    int getLastNote() const noexcept { return lastNote.load (std::memory_order_relaxed); }
    int getLastVelocity() const noexcept { return lastVelocity.load (std::memory_order_relaxed); }

    int countKeysDown() const noexcept
    {
        int count = 0;
        for (int i = 0; i < 128; ++i)
            if (isDown (i))
                ++count;
        return count;
    }

    void apply (const MidiEvent& e) noexcept
    {
        switch (e.type)
        {
            case MidiEventType::NoteOn:
                if (e.note < 128)
                {
                    velocities[e.note].store (e.velocity, std::memory_order_relaxed);
                    lastNote.store (e.note, std::memory_order_relaxed);
                    lastVelocity.store (e.velocity, std::memory_order_relaxed);
                }
                break;

            case MidiEventType::NoteOff:
                if (e.note < 128)
                    velocities[e.note].store (0, std::memory_order_relaxed);
                break;

            case MidiEventType::Sustain:    sustain.store (e.value >= 64, std::memory_order_relaxed); break;
            case MidiEventType::ModWheel:   modWheel.store (e.value, std::memory_order_relaxed); break;
            case MidiEventType::PitchBend:  pitchBend.store (e.value, std::memory_order_relaxed); break;
            case MidiEventType::AllNotesOff: releaseAll(); break;
        }
    }

    void releaseAll() noexcept
    {
        for (auto& v : velocities)
            v.store (0, std::memory_order_relaxed);
        sustain.store (false, std::memory_order_relaxed);
    }

    void reset() noexcept
    {
        releaseAll();
        modWheel.store (0, std::memory_order_relaxed);
        pitchBend.store (0, std::memory_order_relaxed);
        lastNote.store (-1, std::memory_order_relaxed);
        lastVelocity.store (0, std::memory_order_relaxed);
    }

private:
    std::atomic<std::uint8_t> velocities[128];
    std::atomic<bool> sustain { false };
    std::atomic<int> modWheel { 0 };
    std::atomic<int> pitchBend { 0 };
    std::atomic<int> lastNote { -1 };
    std::atomic<int> lastVelocity { 0 };
};

/** Tellere til Feilsøking-visningen. Alle er atomics. */
struct DebugCounters
{
    std::atomic<std::uint32_t> midiNoteOnReceived { 0 };
    std::atomic<std::uint32_t> midiNoteOffReceived { 0 };
    std::atomic<std::uint32_t> noteOnProcessed { 0 };
    std::atomic<std::uint32_t> noteOffProcessed { 0 };
    std::atomic<std::uint32_t> watchdogReleases { 0 };
    std::atomic<int> activeVoices { 0 };

    void reset() noexcept
    {
        midiNoteOnReceived = 0;
        midiNoteOffReceived = 0;
        noteOnProcessed = 0;
        noteOffProcessed = 0;
        watchdogReleases = 0;
    }
};

/** Alt som deles mellom MIDI-tråden, lydmotoren og UI. */
} // namespace kb
