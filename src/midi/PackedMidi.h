#pragma once

#include "midi/MidiHub.h"
#include "midi/MidiParser.h"

#include <cstdint>

namespace kb
{
/** Pakker status og to databytes slik Windows leverer en kort MIDI-melding (lav byte først). */
inline std::uint32_t packMidi (unsigned status, unsigned data1, unsigned data2) noexcept
{
    return static_cast<std::uint32_t> (status & 0xffu)
         | (static_cast<std::uint32_t> (data1 & 0xffu) << 8)
         | (static_cast<std::uint32_t> (data2 & 0xffu) << 16);
}

/** Tolker én ferdig MIDI-melding og legger den i både tangenttilstanden og køen.
    Hver melding behandles for seg, også når flere tangenter slippes i samme øyeblikk. */
inline void applyPackedMidi (std::uint32_t packed, MidiHub& hub) noexcept
{
    const auto status = static_cast<std::uint8_t> (packed & 0xffu);

    if (status < 0x80u || status >= 0xf0u)
        return;

    const auto kind = static_cast<std::uint8_t> (status & 0xf0u);
    const int length = (kind == 0xc0u || kind == 0xd0u) ? 2 : 3;
    const std::uint8_t bytes[3] = {
        status,
        static_cast<std::uint8_t> ((packed >> 8) & 0xffu),
        static_cast<std::uint8_t> ((packed >> 16) & 0xffu)
    };

    MidiParser parser;
    parser.feed (bytes, static_cast<std::size_t> (length), [&hub] (const MidiEvent& event)
    {
        hub.state.apply (event);

        if (event.type == MidiEventType::NoteOn)
            hub.counters.midiNoteOnReceived.fetch_add (1, std::memory_order_relaxed);
        else if (event.type == MidiEventType::NoteOff)
            hub.counters.midiNoteOffReceived.fetch_add (1, std::memory_order_relaxed);

        hub.fifo.push (event);
    });
}
} // namespace kb
