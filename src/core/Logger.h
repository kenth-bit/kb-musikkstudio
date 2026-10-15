#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <deque>
#include <mutex>

namespace kb
{
enum class LogLevel
{
    Info,
    Warning,
    Error,
    Debug
};

/** Enkel logger. Skriver til fil og husker de siste linjene til loggpanelet.
    MÅ IKKE kalles fra lydtråden (den bruker mutex og filaksess). */
class Logger
{
public:
    static Logger& get();

    void setLogFile (const juce::File& file);
    juce::File getLogFile() const;

    void log (LogLevel level, const juce::String& message);
    void info (const juce::String& message)    { log (LogLevel::Info, message); }
    void warn (const juce::String& message)    { log (LogLevel::Warning, message); }
    void error (const juce::String& message)   { log (LogLevel::Error, message); }
    void debug (const juce::String& message)   { log (LogLevel::Debug, message); }

    juce::StringArray getRecentLines (int maxLines) const;

    /** Øker hver gang en ny linje legges til. UI bruker dette til å se om noe er nytt. */
    int getRevision() const noexcept { return revision.load (std::memory_order_acquire); }

    void clearRecent();

private:
    Logger() = default;

    mutable std::mutex mutex;
    std::deque<juce::String> recent;
    juce::File logFile;
    std::atomic<int> revision { 0 };

    static constexpr int maxRecentLines = 500;
    static constexpr juce::int64 maxFileBytes = 1024 * 1024;
};
} // namespace kb
