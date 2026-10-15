#include "core/Settings.h"
#include "core/Text.h"
#include "core/Logger.h"

namespace kb
{
Settings::Settings (juce::File settingsFile) : file (std::move (settingsFile)) {}

bool Settings::load()
{
    values = new juce::DynamicObject();

    if (! file.existsAsFile())
        return false;

    juce::var parsed;
    const auto result = juce::JSON::parse (file.loadFileAsString(), parsed);

    if (result.failed() || parsed.getDynamicObject() == nullptr)
    {
        Logger::get().warn ("Innstillingsfilen er ødelagt. Bruker standardverdier. ("_nb + file.getFullPathName() + ")");
        file.copyFileTo (file.withFileExtension (".bad"));
        return false;
    }

    if (auto* object = parsed.getDynamicObject())
        for (const auto& property : object->getProperties())
            values->setProperty (property.name, property.value);

    return true;
}

bool Settings::save() const
{
    if (file == juce::File())
        return false;

    file.getParentDirectory().createDirectory();
    const auto json = juce::JSON::toString (juce::var (values.get()));
    return file.replaceWithText (json);
}

juce::String Settings::getString (const juce::String& key, const juce::String& defaultValue) const
{
    const auto value = values->getProperty (key);
    return value.isString() ? value.toString() : defaultValue;
}

int Settings::getInt (const juce::String& key, int defaultValue) const
{
    const auto value = values->getProperty (key);
    return (value.isInt() || value.isInt64() || value.isDouble()) ? static_cast<int> (value) : defaultValue;
}

double Settings::getDouble (const juce::String& key, double defaultValue) const
{
    const auto value = values->getProperty (key);
    return (value.isInt() || value.isInt64() || value.isDouble()) ? static_cast<double> (value) : defaultValue;
}

void Settings::setString (const juce::String& key, const juce::String& value) { values->setProperty (key, value); }
void Settings::setInt (const juce::String& key, int value)                       { values->setProperty (key, value); }
void Settings::setDouble (const juce::String& key, double value)                 { values->setProperty (key, value); }
} // namespace kb
