#include "TestFramework.h"
#include "TestUtil.h"

#include "midi/MidiParser.h"

#include <cstdint>
#include <vector>

using namespace kb;

namespace
{
std::vector<MidiEvent> parse (std::initializer_list<int> bytes, MidiParser* existing = nullptr)
{
    MidiParser local;
    MidiParser& parser = existing != nullptr ? *existing : local;

    std::vector<std::uint8_t> data;
    for (const int b : bytes)
        data.push_back (static_cast<std::uint8_t> (b));

    std::vector<MidiEvent> events;
    parser.feed (data.data(), data.size(), [&events] (const MidiEvent& e) { events.push_back (e); });
    return events;
}
} // namespace

KB_TEST (MidiParser, note_on_and_off)
{
    const auto events = parse ({ 0x90, 60, 100, 0x80, 60, 20 });
    KB_CHECK_EQ (events.size(), static_cast<size_t> (2));
    KB_CHECK (events[0].type == MidiEventType::NoteOn);
    KB_CHECK_EQ (events[0].note, static_cast<std::uint8_t> (60));
    KB_CHECK_EQ (events[0].velocity, static_cast<std::uint8_t> (100));
    KB_CHECK (events[1].type == MidiEventType::NoteOff);
    KB_CHECK_EQ (events[1].note, static_cast<std::uint8_t> (60));
}

KB_TEST (MidiParser, note_on_with_velocity_zero_is_note_off)
{
    const auto events = parse ({ 0x90, 64, 0 });
    KB_CHECK_EQ (events.size(), static_cast<size_t> (1));
    KB_CHECK (events[0].type == MidiEventType::NoteOff);
    KB_CHECK_EQ (events[0].note, static_cast<std::uint8_t> (64));
}

KB_TEST (MidiParser, channel_is_decoded)
{
    const auto events = parse ({ 0x95, 60, 100 });
    KB_CHECK_EQ (events.size(), static_cast<size_t> (1));
    KB_CHECK_EQ (events[0].channel, static_cast<std::uint8_t> (5));
}

KB_TEST (MidiParser, running_status)
{
    const auto events = parse ({ 0x90, 60, 100, 64, 90, 67, 80, 60, 0 });
    KB_CHECK_EQ (events.size(), static_cast<size_t> (4));
    KB_CHECK (events[1].type == MidiEventType::NoteOn);
    KB_CHECK_EQ (events[1].note, static_cast<std::uint8_t> (64));
    KB_CHECK (events[2].type == MidiEventType::NoteOn);
    KB_CHECK_EQ (events[2].note, static_cast<std::uint8_t> (67));
    KB_CHECK (events[3].type == MidiEventType::NoteOff);
    KB_CHECK_EQ (events[3].note, static_cast<std::uint8_t> (60));
}

KB_TEST (MidiParser, running_status_survives_split_messages)
{
    MidiParser parser;
    auto events = parse ({ 0x90, 60 }, &parser);
    KB_CHECK (events.empty());
    events = parse ({ 100, 62 }, &parser);
    KB_CHECK_EQ (events.size(), static_cast<size_t> (1));
    events = parse ({ 100 }, &parser);
    KB_CHECK_EQ (events.size(), static_cast<size_t> (1));
    KB_CHECK_EQ (events[0].note, static_cast<std::uint8_t> (62));
}

KB_TEST (MidiParser, realtime_messages_are_ignored_even_in_the_middle)
{
    const auto events = parse ({ 0xF8, 0x90, 0xF8, 60, 0xFE, 100, 0xF8, 0xFA, 0xFC });
    KB_CHECK_EQ (events.size(), static_cast<size_t> (1));
    KB_CHECK (events[0].type == MidiEventType::NoteOn);
    KB_CHECK_EQ (events[0].note, static_cast<std::uint8_t> (60));
    KB_CHECK_EQ (events[0].velocity, static_cast<std::uint8_t> (100));
}

KB_TEST (MidiParser, sysex_is_skipped_and_does_not_confuse_the_stream)
{
    const auto events = parse ({ 0x90, 60, 100, 0xF0, 0x7E, 0x00, 0x09, 0x01, 0x40, 0x40, 0xF7, 0x90, 62, 90 });
    KB_CHECK_EQ (events.size(), static_cast<size_t> (2));
    KB_CHECK_EQ (events[1].note, static_cast<std::uint8_t> (62));
}

KB_TEST (MidiParser, sysex_data_is_not_read_as_running_status)
{
    const auto events = parse ({ 0x90, 60, 100, 0xF0, 61, 100, 62, 100, 0xF7, 63, 100 });
    KB_CHECK_EQ (events.size(), static_cast<size_t> (1));
}

KB_TEST (MidiParser, sustain_pedal_thresholds)
{
    auto events = parse ({ 0xB0, 64, 127, 0xB0, 64, 0, 0xB0, 64, 63, 0xB0, 64, 64 });
    KB_CHECK_EQ (events.size(), static_cast<size_t> (4));
    KB_CHECK (events[0].type == MidiEventType::Sustain && events[0].value == 127);
    KB_CHECK (events[1].type == MidiEventType::Sustain && events[1].value == 0);
    KB_CHECK (events[2].type == MidiEventType::Sustain && events[2].value == 0);
    KB_CHECK (events[3].type == MidiEventType::Sustain && events[3].value == 127);
}

KB_TEST (MidiParser, mod_wheel_and_all_notes_off)
{
    const auto events = parse ({ 0xB0, 1, 77, 0xB0, 123, 0, 0xB0, 120, 0 });
    KB_CHECK_EQ (events.size(), static_cast<size_t> (3));
    KB_CHECK (events[0].type == MidiEventType::ModWheel && events[0].value == 77);
    KB_CHECK (events[1].type == MidiEventType::AllNotesOff);
    KB_CHECK (events[2].type == MidiEventType::AllNotesOff);
}

KB_TEST (MidiParser, unknown_controllers_are_ignored)
{
    const auto events = parse ({ 0xB0, 7, 100, 0xB0, 10, 64, 0xB0, 91, 50 });
    KB_CHECK (events.empty());
}

KB_TEST (MidiParser, pitch_bend_range)
{
    auto events = parse ({ 0xE0, 0x00, 0x40 });
    KB_CHECK_EQ (events.size(), static_cast<size_t> (1));
    KB_CHECK_EQ (events[0].value, static_cast<std::int16_t> (0));

    events = parse ({ 0xE0, 0x7F, 0x7F });
    KB_CHECK_EQ (events[0].value, static_cast<std::int16_t> (8191));

    events = parse ({ 0xE0, 0x00, 0x00 });
    KB_CHECK_EQ (events[0].value, static_cast<std::int16_t> (-8192));
}

KB_TEST (MidiParser, one_byte_messages_do_not_eat_following_data)
{
    const auto events = parse ({ 0xC0, 5, 0xD0, 40, 0x90, 60, 100 });
    KB_CHECK_EQ (events.size(), static_cast<size_t> (1));
    KB_CHECK (events[0].type == MidiEventType::NoteOn);
}

KB_TEST (MidiParser, truncated_message_is_dropped_and_next_status_recovers)
{
    const auto events = parse ({ 0x90, 60, 0x90, 62, 100 });
    KB_CHECK_EQ (events.size(), static_cast<size_t> (1));
    KB_CHECK_EQ (events[0].note, static_cast<std::uint8_t> (62));
}

KB_TEST (MidiParser, data_bytes_without_status_are_ignored)
{
    const auto events = parse ({ 60, 100, 62, 100 });
    KB_CHECK (events.empty());
}

KB_TEST (MidiParser, system_common_clears_running_status)
{
    const auto events = parse ({ 0x90, 60, 100, 0xF1, 0x20, 62, 100 });
    KB_CHECK_EQ (events.size(), static_cast<size_t> (1));
}

KB_TEST (MidiParser, random_garbage_never_crashes_and_gives_valid_events)
{
    kbtest::Rng rng (7);
    MidiParser parser;
    std::vector<std::uint8_t> data (4096);

    for (int round = 0; round < 200; ++round)
    {
        for (auto& b : data)
            b = static_cast<std::uint8_t> (rng.next() & 0xFF);

        parser.feed (data.data(), data.size(), [] (const MidiEvent& e)
        {
            KB_CHECK (e.note < 128);
            KB_CHECK (e.velocity < 128);
            KB_CHECK (e.channel < 16);
        });
    }
}
