#pragma once

#include <juce_core/juce_core.h>

namespace kb
{
/** Små innstillinger lagret som JSON. Mangler filen eller er den ødelagt,
    brukes standardverdiene. Skal bare brukes fra meldingstråden (UI-tråden). */
class Settings
{
public:
    explicit Settings (juce::File settingsFile);

    /** Returnerer true hvis en gyldig fil ble lest. Ellers brukes standardverdier. */
    bool load();
    bool save() const;

    juce::String getString (const juce::String& key, const juce::String& defaultValue = {}) const;
    int getInt (const juce::String& key, int defaultValue) const;
    double getDouble (const juce::String& key, double defaultValue) const;

    void setString (const juce::String& key, const juce::String& value);
    void setInt (const juce::String& key, int value);
    void setDouble (const juce::String& key, double value);

    juce::File getFile() const { return file; }

private:
    juce::File file;
    juce::DynamicObject::Ptr values { new juce::DynamicObject() };
};
} // namespace kb
