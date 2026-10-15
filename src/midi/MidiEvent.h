#pragma once

#include <cstdint>

namespace kb
{
enum class MidiEventType : std::uint8_t
{
    NoteOn,
    NoteOff,
    Sustain,      // value: 0 = av, 127 = på
    ModWheel,     // value: 0..127
    PitchBend,    // value: -8192..8191
    AllNotesOff
};

/** Liten hendelse som er trygg å kopiere på lydtråden (ingen minneallokering). */
struct MidiEvent
{
    MidiEventType type = MidiEventType::NoteOff;
    std::uint8_t channel = 0;
    std::uint8_t note = 0;        // notenummer for NoteOn/NoteOff
    std::uint8_t velocity = 0;    // 0..127 for NoteOn/NoteOff
    std::int16_t value = 0;       // verdi for Sustain/ModWheel/PitchBend
    int sampleOffset = 0;         // plassering i lydblokken (0 = starten)

    static MidiEvent noteOn (int note, int velocity, int channel = 0, int offset = 0) noexcept
    {
        MidiEvent e;
        e.type = MidiEventType::NoteOn;
        e.channel = static_cast<std::uint8_t> (channel);
        e.note = static_cast<std::uint8_t> (note);
        e.velocity = static_cast<std::uint8_t> (velocity);
        e.sampleOffset = offset;
        return e;
    }

    static MidiEvent noteOff (int note, int channel = 0, int offset = 0) noexcept
    {
        MidiEvent e;
        e.type = MidiEventType::NoteOff;
        e.channel = static_cast<std::uint8_t> (channel);
        e.note = static_cast<std::uint8_t> (note);
        e.sampleOffset = offset;
        return e;
    }

    static MidiEvent sustain (bool down, int offset = 0) noexcept
    {
        MidiEvent e;
        e.type = MidiEventType::Sustain;
        e.value = down ? 127 : 0;
        e.sampleOffset = offset;
        return e;
    }

    static MidiEvent modWheel (int value, int offset = 0) noexcept
    {
        MidiEvent e;
        e.type = MidiEventType::ModWheel;
        e.value = static_cast<std::int16_t> (value);
        e.sampleOffset = offset;
        return e;
    }

    static MidiEvent allNotesOff (int offset = 0) noexcept
    {
        MidiEvent e;
        e.type = MidiEventType::AllNotesOff;
        e.sampleOffset = offset;
        return e;
    }
};
} // namespace kb
