#include "TestFramework.h"

#include "midi/MidiEventFifo.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace kb;

KB_TEST (Fifo, keeps_order)
{
    MidiEventFifo fifo (16);
    for (int i = 0; i < 10; ++i)
        KB_CHECK (fifo.push (MidiEvent::noteOn (i, 100)));

    MidiEvent out[16];
    KB_CHECK_EQ (fifo.pop (out, 16), 10);
    for (int i = 0; i < 10; ++i)
        KB_CHECK_EQ (static_cast<int> (out[i].note), i);
    KB_CHECK_EQ (fifo.pop (out, 16), 0);
}

KB_TEST (Fifo, wraps_around_many_times)
{
    MidiEventFifo fifo (8);
    MidiEvent out[8];
    int nextWrite = 0;
    int nextRead = 0;

    for (int round = 0; round < 1000; ++round)
    {
        const int count = 1 + round % 7;
        for (int i = 0; i < count; ++i)
            KB_CHECK (fifo.push (MidiEvent::noteOn (nextWrite++ % 128, 64)));

        const int got = fifo.pop (out, 8);
        KB_CHECK_EQ (got, count);
        for (int i = 0; i < got; ++i)
            KB_CHECK_EQ (static_cast<int> (out[i].note), nextRead++ % 128);
    }

    KB_CHECK_EQ (fifo.getDroppedCount(), 0u);
}

KB_TEST (Fifo, full_queue_drops_new_events_and_counts_them)
{
    MidiEventFifo fifo (4);

    int accepted = 0;
    for (int i = 0; i < 10; ++i)
        accepted += fifo.push (MidiEvent::noteOn (i, 100)) ? 1 : 0;

    KB_CHECK_EQ (accepted, 4);
    KB_CHECK_EQ (fifo.getDroppedCount(), 6u);

    MidiEvent out[8];
    KB_CHECK_EQ (fifo.pop (out, 8), 4);
    KB_CHECK_EQ (static_cast<int> (out[0].note), 0);
    KB_CHECK_EQ (static_cast<int> (out[3].note), 3);

    KB_CHECK (fifo.push (MidiEvent::noteOn (99, 100)));
}

KB_TEST (Fifo, pop_respects_max_count)
{
    MidiEventFifo fifo (16);
    for (int i = 0; i < 10; ++i)
        fifo.push (MidiEvent::noteOn (i, 100));

    MidiEvent out[4];
    KB_CHECK_EQ (fifo.pop (out, 4), 4);
    KB_CHECK_EQ (fifo.getNumReady(), 6);
    KB_CHECK_EQ (fifo.pop (out, 4), 4);
    KB_CHECK_EQ (fifo.pop (out, 4), 2);
}

KB_TEST (Fifo, one_producer_one_consumer_keeps_order_and_accounts_for_every_event)
{
    constexpr int total = 400000;
    MidiEventFifo fifo (64);
    std::atomic<bool> producerDone { false };
    std::atomic<int> accepted { 0 };

    std::thread producer ([&]
    {
        for (int i = 0; i < total; ++i)
        {
            auto e = MidiEvent::noteOn (0, 1);
            e.sampleOffset = i;
            if (fifo.push (e))
                ++accepted;
        }
        producerDone = true;
    });

    int received = 0;
    int lastSeen = -1;
    bool ordered = true;
    MidiEvent out[32];

    while (true)
    {
        const int got = fifo.pop (out, 32);
        for (int i = 0; i < got; ++i)
        {
            if (out[i].sampleOffset <= lastSeen)
                ordered = false;
            lastSeen = out[i].sampleOffset;
        }
        received += got;

        if (got == 0 && producerDone.load() && fifo.getNumReady() == 0)
            break;
    }

    producer.join();

    KB_CHECK (ordered);
    KB_CHECK_EQ (received, accepted.load());
    KB_CHECK_EQ (static_cast<std::uint32_t> (accepted.load()) + fifo.getDroppedCount(), static_cast<std::uint32_t> (total));
}

KB_TEST (Fifo, several_producers_never_corrupt_or_block)
{
    constexpr int perProducer = 100000;
    constexpr int numProducers = 3;
    MidiEventFifo fifo (128);
    std::atomic<int> producersDone { 0 };
    std::atomic<int> accepted { 0 };

    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p)
    {
        producers.emplace_back ([&, p]
        {
            for (int i = 0; i < perProducer; ++i)
            {
                auto e = MidiEvent::noteOn (p, 1);
                e.sampleOffset = i;
                if (fifo.push (e))
                    ++accepted;
            }
            ++producersDone;
        });
    }

    int received = 0;
    int last[numProducers] = { -1, -1, -1 };
    bool ordered = true;
    MidiEvent out[64];

    while (true)
    {
        const int got = fifo.pop (out, 64);
        for (int i = 0; i < got; ++i)
        {
            const int p = out[i].note;
            if (p < 0 || p >= numProducers || out[i].sampleOffset <= last[p])
                ordered = false;
            else
                last[p] = out[i].sampleOffset;
        }
        received += got;

        if (got == 0 && producersDone.load() == numProducers && fifo.getNumReady() == 0)
            break;
    }

    for (auto& t : producers)
        t.join();

    KB_CHECK (ordered);
    KB_CHECK_EQ (received, accepted.load());
    KB_CHECK_EQ (static_cast<std::uint32_t> (accepted.load()) + fifo.getDroppedCount(),
                 static_cast<std::uint32_t> (perProducer * numProducers));
}
