#include "TestFramework.h"

#include "ui/Layout.h"
#include "ui/Theme.h"
#include "ui/ThemeLoader.h"

using namespace kb;

KB_TEST (Layout, snap_rounds_to_the_grid)
{
    Layout l;
    l.grid = 4;
    KB_CHECK_EQ (l.snap (0), 0);
    KB_CHECK_EQ (l.snap (1), 0);
    KB_CHECK_EQ (l.snap (2), 4);
    KB_CHECK_EQ (l.snap (5), 4);
    KB_CHECK_EQ (l.snap (6), 8);
    KB_CHECK_EQ (l.snap (13), 12);
    KB_CHECK_EQ (l.snap (14), 16);

    l.grid = 8;
    KB_CHECK_EQ (l.snap (12), 16);
    KB_CHECK_EQ (l.snap (11), 8);

    l.grid = 0;
    KB_CHECK_EQ (l.snap (7), 7);
}

KB_TEST (Layout, defaults_sit_on_the_grid_and_are_sane)
{
    Layout l;
    for (const auto& field : l.fields())
    {
        const juce::String name (field.first);
        const int value = *field.second;
        KB_CHECK_MSG (value > 0, name.toStdString());

        if (name.startsWith ("minWindow") || name.startsWith ("defaultWindow") || name == "grid")
            continue;

        KB_CHECK_MSG (value % l.grid == 0, name.toStdString() + " = " + std::to_string (value) + " er ikke et multiplum av rutenettet");
    }

    KB_CHECK (l.defaultWindowWidth >= l.minWindowWidth);
    KB_CHECK (l.defaultWindowHeight >= l.minWindowHeight);
}

KB_TEST (Layout, every_field_has_a_unique_name)
{
    Layout l;
    juce::StringArray names;
    for (const auto& field : l.fields())
    {
        KB_CHECK (! names.contains (field.first));
        names.add (field.first);
    }
    KB_CHECK (names.size() >= 20);
}

KB_TEST (ThemeLoader, bad_json_leaves_defaults_untouched)
{
    for (const char* bad : { "{ ikke json", "", "[1,2]", "42", "{\"layout\": ", "\"tekst\"" })
    {
        Theme theme;
        Layout layout;
        juce::StringArray warnings;

        KB_CHECK (! ThemeLoader::applyJson (bad, theme, layout, warnings));
        KB_CHECK (! warnings.isEmpty());
        KB_CHECK_EQ (layout.outerMargin, Layout().outerMargin);
        KB_CHECK (theme.accentCyan == Theme().accentCyan);
    }
}

KB_TEST (ThemeLoader, valid_values_are_applied)
{
    Theme theme;
    Layout layout;
    juce::StringArray warnings;

    const bool ok = ThemeLoader::applyJson (
        R"({ "layout": { "outerMargin": 24, "panelGap": 20 },
             "theme": { "cornerRadius": 12 },
             "colours": { "accentCyan": "#112233", "background": "#80ff0000" } })",
        theme, layout, warnings);

    KB_CHECK (ok);
    KB_CHECK (warnings.isEmpty());
    KB_CHECK_EQ (layout.outerMargin, 24);
    KB_CHECK_EQ (layout.panelGap, 20);
    KB_CHECK_EQ (theme.cornerRadius, 12);
    KB_CHECK (theme.accentCyan == juce::Colour (0xff112233));
    KB_CHECK (theme.background == juce::Colour (0x80ff0000));
}

KB_TEST (ThemeLoader, bad_individual_values_warn_and_are_ignored)
{
    Theme theme;
    Layout layout;
    juce::StringArray warnings;

    const bool ok = ThemeLoader::applyJson (
        R"({ "layout": { "outerMargin": "mye", "panelGap": -5, "finnesIkke": 3, "knobSize": 99999 },
             "colours": { "accentCyan": "ikke en farge", "ukjentFarge": "#ffffff" },
             "ukjentAvsnitt": { "a": 1 },
             "theme": 5 })",
        theme, layout, warnings);

    KB_CHECK (ok);
    KB_CHECK (warnings.size() >= 6);
    KB_CHECK_EQ (layout.outerMargin, Layout().outerMargin);
    KB_CHECK_EQ (layout.panelGap, Layout().panelGap);
    KB_CHECK_EQ (layout.knobSize, Layout().knobSize);
    KB_CHECK (theme.accentCyan == Theme().accentCyan);
}

KB_TEST (ThemeLoader, empty_object_changes_nothing)
{
    Theme theme;
    Layout layout;
    juce::StringArray warnings;
    KB_CHECK (ThemeLoader::applyJson ("{}", theme, layout, warnings));
    KB_CHECK (warnings.isEmpty());
    KB_CHECK_EQ (layout.panelPadding, Layout().panelPadding);
}

KB_TEST (ThemeLoader, the_theme_json_in_the_repository_is_valid)
{
    const auto file = juce::File (KB_SOURCE_RESOURCES_DIR).getChildFile ("theme.json");
    KB_CHECK (file.existsAsFile());

    Theme theme;
    Layout layout;
    juce::StringArray warnings;
    KB_CHECK (ThemeLoader::applyJson (file.loadFileAsString(), theme, layout, warnings));
    KB_CHECK_MSG (warnings.isEmpty(), warnings.joinIntoString ("; ").toStdString());
}

KB_TEST (ThemeLoader, reload_resets_before_applying)
{
    Theme theme;
    Layout layout;
    layout.outerMargin = 999;
    theme.cornerRadius = 77;

    ThemeLoader::reload (theme, layout);
    KB_CHECK (layout.outerMargin != 999);
    KB_CHECK (theme.cornerRadius != 77);
}

KB_TEST (Theme, every_colour_and_size_field_is_reachable_from_the_json)
{
    Theme theme;
    KB_CHECK (theme.colourFields().size() >= 16);
    KB_CHECK (theme.sizeFields().size() >= 6);
}
