#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <atomic>
#include <map>
#include <memory>
#include <vector>

namespace kb
{
enum class ParamFormat
{
    Plain,
    Time,       // verdi i sekunder, vises som ms eller s
    Cents,
    Decibel,
    Percent     // verdi 0..1 vises som 0..100 %
};

/** Alt som beskriver en knott. Hjelpeteksten ligger her og er den ENESTE kilden til
    forklaringen i hjelpelinjen og i docs/KNAPPEGUIDE.md. */
struct ParamInfo
{
    juce::String id;
    juce::String name;
    juce::String group;
    juce::String help;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    float skew = 1.0f;
    ParamFormat format = ParamFormat::Plain;
};

/** Én parameter. Verdien er en atomic så lydtråden kan lese den uten låser. */
class Parameter
{
public:
    explicit Parameter (ParamInfo infoToUse);

    const ParamInfo& info() const noexcept { return paramInfo; }
    const juce::NormalisableRange<float>& getRange() const noexcept { return range; }

    float get() const noexcept { return value.load (std::memory_order_relaxed); }
    void set (float newValue) noexcept;
    void reset() noexcept { set (paramInfo.defaultValue); }

    juce::String format (float v) const;
    juce::String formatCurrent() const { return format (get()); }

private:
    ParamInfo paramInfo;
    juce::NormalisableRange<float> range;
    std::atomic<float> value;
};

/** Hjelpetekst for kontroller som ikke er knotter (knapper, nedtrekksmenyer, målere ...). */
struct ControlHelp
{
    juce::String id;
    juce::String name;
    juce::String group;
    juce::String help;
};

class ParameterRegistry
{
public:
    Parameter& addParameter (ParamInfo info);
    void addControlHelp (ControlHelp help);

    Parameter* find (const juce::String& id) noexcept;
    const Parameter* find (const juce::String& id) const noexcept;

    /** Returnerer parameteren, eller en reserveparameter hvis id-en ikke finnes (aldri nullptr). */
    Parameter& require (const juce::String& id);

    juce::String getHelp (const juce::String& id) const;
    juce::String getName (const juce::String& id) const;

    std::vector<const Parameter*> allParameters() const;
    const std::vector<ControlHelp>& allControls() const noexcept { return controls; }

private:
    std::vector<std::unique_ptr<Parameter>> parameters;
    std::map<juce::String, Parameter*> byId;
    std::vector<ControlHelp> controls;
    Parameter fallback { ParamInfo { "ukjent", "Ukjent", {}, {}, 0.0f, 1.0f, 0.0f, 1.0f, ParamFormat::Plain } };
};
} // namespace kb
