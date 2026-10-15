#include "core/CrashHandler.h"

namespace kb
{
namespace
{
juce::File crashFile;

void crashCallback (void*)
{
    if (crashFile == juce::File())
        return;

    juce::String report;
    report << "KB Musikkstudio krasjet " << juce::Time::getCurrentTime().toString (true, true) << "\n\n"
           << juce::SystemStats::getStackBacktrace() << "\n";
    crashFile.appendText (report);
}
} // namespace

void installCrashHandler (const juce::File& crashLogFile)
{
    crashFile = crashLogFile;
    crashFile.getParentDirectory().createDirectory();
    juce::SystemStats::setApplicationCrashHandler (crashCallback);
}
} // namespace kb
