#pragma once

#include "midi/MidiEvent.h"

#include <cstddef>
#include <cstdint>

namespace kb
{
/** Tolker rå MIDI-bytes til MidiEvent. Ren logikk uten maskinvare.
    Støtter running status, Note On med velocity 0 (= Note Off), sanntidsmeldinger midt i
    andre meldinger (Clock, Active Sensing ...) og hopper over SysEx.
    Avkuttede eller ugyldige meldinger forkastes uten å krasje. */
class MidiParser
{
public:
    /** Mater bytes inn. `emit` kalles som emit(const MidiEvent&) for hver ferdig tolket hendelse. */
    template <typename Emit>
    void feed (const std::uint8_t* data, std::size_t numBytes, Emit&& emit)
    {
        for (std::size_t i = 0; i < numBytes; ++i)
            feedByte (data[i], emit);
    }

    void reset() noexcept
    {
        runningStatus = 0;
        dataCount = 0;
        inSysEx = false;
    }

private:
    template <typename Emit>
    void feedByte (std::uint8_t byte, Emit& emit)
    {
        if (byte >= 0xF8)
            return; // sanntidsmeldinger (Clock, Active Sensing, ...) ignoreres og påvirker ikke noe annet

        if (byte & 0x80)
        {
            if (byte == 0xF0)
            {
                inSysEx = true;
                runningStatus = 0;
                dataCount = 0;
                return;
            }

            if (byte == 0xF7)
            {
                inSysEx = false;
                return;
            }

            inSysEx = false;
            dataCount = 0;

            if (byte >= 0xF1)
            {
                runningStatus = 0; // system common nullstiller running status
                return;
            }

            runningStatus = byte;
            return;
        }

        if (inSysEx || runningStatus == 0)
            return;

        data[dataCount++] = byte;

        const auto kind = static_cast<std::uint8_t> (runningStatus & 0xF0);
        const int needed = (kind == 0xC0 || kind == 0xD0) ? 1 : 2;

        if (dataCount < needed)
            return;

        dataCount = 0;
        dispatch (kind, static_cast<std::uint8_t> (runningStatus & 0x0F), data[0], data[1], emit);
    }

    template <typename Emit>
    static void dispatch (std::uint8_t kind, std::uint8_t channel, std::uint8_t d1, std::uint8_t d2, Emit& emit)
    {
        MidiEvent e;
        e.channel = channel;

        switch (kind)
        {
            case 0x90:
                e.note = d1;
                e.velocity = d2;
                e.type = d2 > 0 ? MidiEventType::NoteOn : MidiEventType::NoteOff;
                emit (e);
                break;

            case 0x80:
                e.type = MidiEventType::NoteOff;
                e.note = d1;
                e.velocity = d2;
                emit (e);
                break;

            case 0xB0:
                if (d1 == 64)
                {
                    e.type = MidiEventType::Sustain;
                    e.value = d2 >= 64 ? 127 : 0;
                    emit (e);
                }
                else if (d1 == 1)
                {
                    e.type = MidiEventType::ModWheel;
                    e.value = d2;
                    emit (e);
                }
                else if (d1 == 123 || d1 == 120)
                {
                    e.type = MidiEventType::AllNotesOff;
                    emit (e);
                }
                break;

            case 0xE0:
                e.type = MidiEventType::PitchBend;
                e.value = static_cast<std::int16_t> (((static_cast<int> (d2) << 7) | d1) - 8192);
                emit (e);
                break;

            default:
                break;
        }
    }

    std::uint8_t runningStatus = 0;
    std::uint8_t data[2] = { 0, 0 };
    int dataCount = 0;
    bool inSysEx = false;
};
} // namespace kb
