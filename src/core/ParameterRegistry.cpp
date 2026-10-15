#include "core/ParameterRegistry.h"

namespace kb
{
static_assert (std::atomic<float>::is_always_lock_free, "Parameters must be lock-free");

Parameter::Parameter (ParamInfo infoToUse)
    : paramInfo (std::move (infoToUse)),
      range (paramInfo.minValue, paramInfo.maxValue, 0.0f, paramInfo.skew),
      value (paramInfo.defaultValue)
{
}

void Parameter::set (float newValue) noexcept
{
    if (! std::isfinite (newValue))
        return;

    value.store (juce::jlimit (paramInfo.minValue, paramInfo.maxValue, newValue), std::memory_order_relaxed);
}

juce::String Parameter::format (float v) const
{
    switch (paramInfo.format)
    {
        case ParamFormat::Time:
            return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms"
                            : juce::String (v, 2) + " s";
        case ParamFormat::Cents:
            return juce::String (juce::roundToInt (v)) + " cent";
        case ParamFormat::Decibel:
            return (v <= -59.9f ? juce::String ("-inf") : juce::String (v, 1)) + " dB";
        case ParamFormat::Percent:
            return juce::String (juce::roundToInt (v * 100.0f)) + " %";
        case ParamFormat::Plain:
            break;
    }
    return juce::String (v, 2);
}

Parameter& ParameterRegistry::addParameter (ParamInfo info)
{
    jassert (find (info.id) == nullptr);
    auto parameter = std::make_unique<Parameter> (std::move (info));
    auto& ref = *parameter;
    byId[ref.info().id] = &ref;
    parameters.push_back (std::move (parameter));
    return ref;
}

void ParameterRegistry::addControlHelp (ControlHelp help)
{
    controls.push_back (std::move (help));
}

Parameter* ParameterRegistry::find (const juce::String& id) noexcept
{
    const auto it = byId.find (id);
    return it == byId.end() ? nullptr : it->second;
}

const Parameter* ParameterRegistry::find (const juce::String& id) const noexcept
{
    const auto it = byId.find (id);
    return it == byId.end() ? nullptr : it->second;
}

Parameter& ParameterRegistry::require (const juce::String& id)
{
    if (auto* p = find (id))
        return *p;

    jassertfalse;
    return fallback;
}

juce::String ParameterRegistry::getHelp (const juce::String& id) const
{
    if (const auto* p = find (id))
        return p->info().help;

    for (const auto& c : controls)
        if (c.id == id)
            return c.help;

    return {};
}

juce::String ParameterRegistry::getName (const juce::String& id) const
{
    if (const auto* p = find (id))
        return p->info().name;

    for (const auto& c : controls)
        if (c.id == id)
            return c.name;

    return id;
}

std::vector<const Parameter*> ParameterRegistry::allParameters() const
{
    std::vector<const Parameter*> result;
    result.reserve (parameters.size());
    for (const auto& p : parameters)
        result.push_back (p.get());
    return result;
}
} // namespace kb
