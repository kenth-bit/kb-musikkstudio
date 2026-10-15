#pragma once

#include "midi/MidiEvent.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <vector>

namespace kb
{
/** Låsfri kø fra MIDI-tråden til lydtråden.

    - Skriving (push) kan skje fra flere MIDI-tråder, men er beskyttet av en liten spinlås
      slik at AbstractFifo alltid har bare én skriver. Lydtråden berøres aldri av låsen.
    - Lesing (pop) skjer bare fra lydtråden og tar aldri lås eller allokerer.
    - Er køen full, forkastes den nye hendelsen og en teller økes. Kritiske hendelser
      (tangent opp) blir likevel ikke til hengende toner, fordi synthen også sjekker
      tangenttilstanden i MidiState. */
class MidiEventFifo
{
public:
    /** Rommer `capacity` hendelser. (AbstractFifo trenger én ledig plass, derfor + 1.) */
    explicit MidiEventFifo (int capacity = 4096)
        : fifo (capacity + 1), storage (static_cast<size_t> (capacity + 1)) {}

    bool push (const MidiEvent& event) noexcept
    {
        const juce::SpinLock::ScopedLockType lock (writeLock);

        int start1, size1, start2, size2;
        fifo.prepareToWrite (1, start1, size1, start2, size2);

        if (size1 + size2 < 1)
        {
            dropped.fetch_add (1, std::memory_order_relaxed);
            return false;
        }

        storage[static_cast<size_t> (size1 > 0 ? start1 : start2)] = event;
        fifo.finishedWrite (1);
        return true;
    }

    /** Henter opptil maxCount hendelser i riktig rekkefølge. Returnerer antall hentet. */
    int pop (MidiEvent* destination, int maxCount) noexcept
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (maxCount, start1, size1, start2, size2);

        int written = 0;
        for (int i = 0; i < size1; ++i)
            destination[written++] = storage[static_cast<size_t> (start1 + i)];
        for (int i = 0; i < size2; ++i)
            destination[written++] = storage[static_cast<size_t> (start2 + i)];

        fifo.finishedRead (size1 + size2);
        return written;
    }

    int getNumReady() const noexcept { return fifo.getNumReady(); }
    std::uint32_t getDroppedCount() const noexcept { return dropped.load (std::memory_order_relaxed); }

private:
    juce::AbstractFifo fifo;
    std::vector<MidiEvent> storage;
    juce::SpinLock writeLock;
    std::atomic<std::uint32_t> dropped { 0 };
};
} // namespace kb
