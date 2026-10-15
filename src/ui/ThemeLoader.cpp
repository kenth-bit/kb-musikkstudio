#include "ui/ThemeLoader.h"
#include "core/Text.h"

#include "core/Logger.h"

#include <juce_core/juce_core.h>

namespace kb
{
namespace
{
juce::Colour parseColour (const juce::String& text, bool& ok)
{
    auto hex = text.trim().trimCharactersAtStart ("#");
    ok = (hex.length() == 6 || hex.length() == 8) && hex.containsOnly ("0123456789abcdefABCDEF");
    if (! ok)
        return {};

    if (hex.length() == 6)
        hex = "ff" + hex;

    return juce::Colour::fromString (hex);
}
} // namespace

bool ThemeLoader::applyJson (const juce::String& json, Theme& themeToChange, Layout& layoutToChange,
                             juce::StringArray& warnings)
{
    juce::var parsed;
    const auto result = juce::JSON::parse (json, parsed);

    if (result.failed() || parsed.getDynamicObject() == nullptr)
    {
        warnings.add ("theme.json er ikke gyldig JSON. Bruker standardverdier.");
        return false;
    }

    auto* root = parsed.getDynamicObject();

    for (const auto& property : root->getProperties())
    {
        const auto section = property.name.toString();
        auto* object = property.value.getDynamicObject();

        if (object == nullptr)
        {
            warnings.add ("theme.json: \"" + section + "\" skal være et objekt."_nb);
            continue;
        }

        if (section == "layout")
        {
            auto fields = layoutToChange.fields();
            for (const auto& entry : object->getProperties())
            {
                const auto name = entry.name.toString();
                bool found = false;

                for (auto& field : fields)
                {
                    if (name == field.first)
                    {
                        found = true;
                        if (entry.value.isInt() || entry.value.isInt64() || entry.value.isDouble())
                        {
                            const int value = juce::roundToInt (static_cast<double> (entry.value));
                            if (value < 0 || value > 4000)
                                warnings.add ("theme.json: layout." + name + " er utenfor området 0-4000."_nb);
                            else
                                *field.second = value;
                        }
                        else
                        {
                            warnings.add ("theme.json: layout." + name + " skal være et tall."_nb);
                        }
                    }
                }

                if (! found)
                    warnings.add ("theme.json: ukjent navn layout." + name);
            }
        }
        else if (section == "theme")
        {
            auto fields = themeToChange.sizeFields();
            for (const auto& entry : object->getProperties())
            {
                const auto name = entry.name.toString();
                bool found = false;

                for (auto& field : fields)
                {
                    if (name == field.first)
                    {
                        found = true;
                        if (entry.value.isInt() || entry.value.isInt64() || entry.value.isDouble())
                            *field.second = juce::jlimit (0, 200, juce::roundToInt (static_cast<double> (entry.value)));
                        else
                            warnings.add ("theme.json: theme." + name + " skal være et tall."_nb);
                    }
                }

                if (! found)
                    warnings.add ("theme.json: ukjent navn theme." + name);
            }
        }
        else if (section == "colours")
        {
            auto fields = themeToChange.colourFields();
            for (const auto& entry : object->getProperties())
            {
                const auto name = entry.name.toString();
                bool found = false;

                for (auto& field : fields)
                {
                    if (name == field.first)
                    {
                        found = true;
                        bool ok = false;
                        const auto colour = parseColour (entry.value.toString(), ok);
                        if (ok)
                            *field.second = colour;
                        else
                            warnings.add ("theme.json: colours." + name + " må være en farge som #RRGGBB."_nb);
                    }
                }

                if (! found)
                    warnings.add ("theme.json: ukjent navn colours." + name);
            }
        }
        else
        {
            warnings.add ("theme.json: ukjent avsnitt \"" + section + "\".");
        }
    }

    return true;
}

juce::File ThemeLoader::findThemeFile()
{
    juce::Array<juce::File> candidates;

#ifdef KB_SOURCE_RESOURCES_DIR
    candidates.add (juce::File (KB_SOURCE_RESOURCES_DIR).getChildFile ("theme.json"));
#endif

    const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    candidates.add (exeDir.getChildFile ("resources").getChildFile ("theme.json"));
    candidates.add (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                        .getChildFile ("KBMusikkstudio").getChildFile ("theme.json"));

    for (const auto& file : candidates)
        if (file.existsAsFile())
            return file;

    return {};
}

bool ThemeLoader::reload (Theme& themeToChange, Layout& layoutToChange)
{
    themeToChange.resetDefaults();
    layoutToChange.resetDefaults();

    const auto file = findThemeFile();
    if (file == juce::File())
    {
        Logger::get().info ("Tema: fant ingen theme.json. Bruker standardverdier.");
        return false;
    }

    juce::StringArray warnings;
    const bool ok = applyJson (file.loadFileAsString(), themeToChange, layoutToChange, warnings);

    for (const auto& w : warnings)
        Logger::get().warn (w);

    if (ok)
        Logger::get().info ("Tema: leste " + file.getFullPathName());
    else
    {
        themeToChange.resetDefaults();
        layoutToChange.resetDefaults();
    }

    return ok;
}
} // namespace kb
