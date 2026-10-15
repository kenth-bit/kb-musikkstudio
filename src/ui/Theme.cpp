#include "ui/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace kb
{
Theme& theme()
{
    static Theme instance;
    return instance;
}

juce::Font Theme::font (int size, bool bold) const
{
    return juce::Font (juce::FontOptions (static_cast<float> (size), bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font Theme::monoFont (int size) const
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), static_cast<float> (size), juce::Font::plain));
}
} // namespace kb
