#include "TestFramework.h"

#include <juce_events/juce_events.h>

#include <chrono>
#include <cstring>

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    std::string filter;
    bool listOnly = false;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp (argv[i], "--list") == 0)
            listOnly = true;
        else
            filter = argv[i];
    }

    auto matches = [&filter] (const std::string& name)
    {
        if (filter.empty())
            return true;
        return name == filter || name.rfind (filter + ".", 0) == 0;
    };

    int run = 0;
    int failedTests = 0;
    const auto start = std::chrono::steady_clock::now();

    for (const auto& test : kbtest::registry())
    {
        if (! matches (test.name))
            continue;

        if (listOnly)
        {
            std::printf ("%s\n", test.name.c_str());
            continue;
        }

        ++run;
        const int failedBefore = kbtest::state().failedChecks.load();
        std::printf ("[ KJØRER ] %s\n", test.name.c_str());
        std::fflush (stdout);

        try
        {
            test.body();
        }
        catch (const std::exception& e)
        {
            kbtest::fail (__FILE__, __LINE__, std::string ("uventet unntak: ") + e.what());
        }
        catch (...)
        {
            kbtest::fail (__FILE__, __LINE__, "uventet unntak");
        }

        const bool failed = kbtest::state().failedChecks.load() != failedBefore;
        failedTests += failed ? 1 : 0;
        std::printf ("[ %s ] %s\n", failed ? "FEILET  " : "OK      ", test.name.c_str());
    }

    if (listOnly)
        return 0;

    if (run == 0)
    {
        std::fprintf (stderr, "Ingen tester matchet \"%s\".\n", filter.c_str());
        return 2;
    }

    const double seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    std::printf ("\n%d tester kjørt, %d feilet, %d kontroller, %.1f sekunder.\n", run, failedTests,
                 kbtest::state().totalChecks.load(), seconds);
    return failedTests == 0 ? 0 : 1;
}
