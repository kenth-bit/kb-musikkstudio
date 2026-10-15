#pragma once

#include <juce_core/juce_core.h>

namespace kb
{
/** Skriver en stakkspor-fil hvis programmet krasjer, så vi kan finne feilen. */
void installCrashHandler (const juce::File& crashLogFile);
}
