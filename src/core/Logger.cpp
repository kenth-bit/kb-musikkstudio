#include "core/Logger.h"

namespace kb
{
Logger& Logger::get()
{
    static Logger instance;
    return instance;
}

void Logger::setLogFile (const juce::File& file)
{
    std::lock_guard<std::mutex> lock (mutex);
    logFile = file;

    if (logFile != juce::File())
    {
        logFile.getParentDirectory().createDirectory();

        if (logFile.existsAsFile() && logFile.getSize() > maxFileBytes)
            logFile.moveFileTo (logFile.withFileExtension (".old"));
    }
}

juce::File Logger::getLogFile() const
{
    std::lock_guard<std::mutex> lock (mutex);
    return logFile;
}

void Logger::log (LogLevel level, const juce::String& message)
{
    const char* tag = "INFO ";
    switch (level)
    {
        case LogLevel::Info:    tag = "INFO "; break;
        case LogLevel::Warning: tag = "ADVAR"; break;
        case LogLevel::Error:   tag = "FEIL "; break;
        case LogLevel::Debug:   tag = "DEBUG"; break;
    }

    const auto line = juce::Time::getCurrentTime().formatted ("%H:%M:%S") + "  " + tag + "  " + message;

    {
        std::lock_guard<std::mutex> lock (mutex);
        recent.push_back (line);
        while (static_cast<int> (recent.size()) > maxRecentLines)
            recent.pop_front();

        if (logFile != juce::File())
            logFile.appendText (juce::Time::getCurrentTime().formatted ("%Y-%m-%d ") + line + "\n");
    }

    revision.fetch_add (1, std::memory_order_release);
}

juce::StringArray Logger::getRecentLines (int maxLines) const
{
    std::lock_guard<std::mutex> lock (mutex);
    juce::StringArray result;
    const int total = static_cast<int> (recent.size());
    for (int i = juce::jmax (0, total - maxLines); i < total; ++i)
        result.add (recent[static_cast<size_t> (i)]);
    return result;
}

void Logger::clearRecent()
{
    {
        std::lock_guard<std::mutex> lock (mutex);
        recent.clear();
    }
    revision.fetch_add (1, std::memory_order_release);
}
} // namespace kb
