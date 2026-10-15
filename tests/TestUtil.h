#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace kbtest
{
/** Midlertidig mappe som slettes når objektet forsvinner. */
class TempDir
{
public:
    TempDir()
        : dir (juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("kb_test_" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64())))
    {
        dir.createDirectory();
    }

    ~TempDir() { dir.deleteRecursively(); }

    juce::File file (const juce::String& name) const { return dir.getChildFile (name); }
    juce::File path() const { return dir; }

private:
    juce::File dir;
};

inline float peakOf (const std::vector<float>& buffer, int count)
{
    float peak = 0.0f;
    for (int i = 0; i < count; ++i)
        peak = std::max (peak, std::fabs (buffer[static_cast<size_t> (i)]));
    return peak;
}

inline bool allFinite (const std::vector<float>& buffer, int count)
{
    for (int i = 0; i < count; ++i)
        if (! std::isfinite (buffer[static_cast<size_t> (i)]))
            return false;
    return true;
}

/** Enkel, deterministisk tilfeldighetsgenerator så en feilet test kan gjentas med samme frø. */
class Rng
{
public:
    explicit Rng (unsigned seed) : s (seed * 2654435761u + 12345u) { next(); next(); }

    unsigned next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }

    int range (int low, int high) { return low + static_cast<int> (next() % static_cast<unsigned> (high - low + 1)); }
    double unit() { return static_cast<double> (next() & 0xFFFFFFu) / 16777216.0; }
    bool chance (double probability) { return unit() < probability; }

    template <typename T>
    const T& pick (const std::vector<T>& values) { return values[next() % values.size()]; }

private:
    unsigned s;
};
} // namespace kbtest
