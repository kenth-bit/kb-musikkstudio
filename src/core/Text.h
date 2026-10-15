#pragma once

#include <juce_core/juce_core.h>

namespace kb
{
/** Norske tegn (æ, ø, å) i tekstbokstaver MÅ skrives med _nb etter anførselstegnet,
    for eksempel "Prøver på nytt"_nb. Uten _nb tolker JUCE teksten som ASCII og viser feil tegn.
    Rene ASCII-tekster trenger ikke _nb. tools/sjekk_tekst.py passer på at ingen glemmer det. */
inline juce::String operator""_nb (const char* text, std::size_t length)
{
    return juce::String::fromUTF8 (text, static_cast<int> (length));
}

/** For tekster som kommer som const char* (for eksempel fra tabeller). */
inline juce::String nb (const char* utf8Text)
{
    return juce::String::fromUTF8 (utf8Text);
}
} // namespace kb
