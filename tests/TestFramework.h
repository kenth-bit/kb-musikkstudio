#pragma once

#include <atomic>
#include <cmath>
#include <cstdio>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

/** Liten testramme uten eksterne avhengigheter. Testnavn skrives "Gruppe.navn".
    Kjør alle:      kb_tests
    Kjør en gruppe: kb_tests Gruppe        (alt som starter med "Gruppe.")
    Vis testene:    kb_tests --list */
namespace kbtest
{
struct TestCase
{
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar
{
    Registrar (const char* name, std::function<void()> body) { registry().push_back ({ name, std::move (body) }); }
};

struct State
{
    std::atomic<int> failedChecks { 0 };
    std::atomic<int> totalChecks { 0 };
    std::mutex printMutex;
    std::string currentTest;
};

inline State& state()
{
    static State s;
    return s;
}

inline void fail (const char* file, int line, const std::string& message)
{
    ++state().failedChecks;
    std::lock_guard<std::mutex> lock (state().printMutex);
    std::fprintf (stderr, "    FEIL %s:%d: %s\n", file, line, message.c_str());
}

template <typename T>
std::string show (const T& value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

inline std::string show (bool value) { return value ? "true" : "false"; }
inline std::string show (unsigned char value) { return std::to_string (static_cast<int> (value)); }
} // namespace kbtest

#define KB_TEST(group, name)                                                                   \
    static void kbtest_##group##_##name();                                                     \
    static kbtest::Registrar kbtest_registrar_##group##_##name (#group "." #name,              \
                                                                kbtest_##group##_##name);      \
    static void kbtest_##group##_##name()

#define KB_CHECK(condition)                                                                    \
    do                                                                                         \
    {                                                                                          \
        ++kbtest::state().totalChecks;                                                         \
        if (! (condition))                                                                     \
            kbtest::fail (__FILE__, __LINE__, "krav ikke oppfylt: " #condition);               \
    } while (false)

#define KB_CHECK_MSG(condition, message)                                                       \
    do                                                                                         \
    {                                                                                          \
        ++kbtest::state().totalChecks;                                                         \
        if (! (condition))                                                                     \
            kbtest::fail (__FILE__, __LINE__, std::string ("krav ikke oppfylt: " #condition ". ") + (message)); \
    } while (false)

#define KB_CHECK_EQ(actual, expected)                                                          \
    do                                                                                         \
    {                                                                                          \
        ++kbtest::state().totalChecks;                                                         \
        const auto kbActual = (actual);                                                        \
        const auto kbExpected = (expected);                                                    \
        if (! (kbActual == kbExpected))                                                        \
            kbtest::fail (__FILE__, __LINE__, std::string (#actual " var ") + kbtest::show (kbActual) \
                                                  + ", forventet " + kbtest::show (kbExpected)); \
    } while (false)

#define KB_CHECK_NEAR(actual, expected, tolerance)                                             \
    do                                                                                         \
    {                                                                                          \
        ++kbtest::state().totalChecks;                                                         \
        const double kbActual = static_cast<double> (actual);                                  \
        const double kbExpected = static_cast<double> (expected);                              \
        if (! (std::fabs (kbActual - kbExpected) <= static_cast<double> (tolerance)))          \
            kbtest::fail (__FILE__, __LINE__, std::string (#actual " var ") + kbtest::show (kbActual) \
                                                  + ", forventet " + kbtest::show (kbExpected) \
                                                  + " +/- " + kbtest::show (static_cast<double> (tolerance))); \
    } while (false)
