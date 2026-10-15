#include "ui/SynthPanel.h"

#include "core/HelpTexts.h"
#include "ui/Components.h"
#include "ui/HelpHub.h"
#include "ui/Layout.h"

#include <memory>
#include <vector>

namespace kb
{
namespace
{
class SynthPanel : public Panel,
                   private juce::Timer
{
public:
    SynthPanel (SynthModule& moduleToControl, ParameterRegistry& registry, DebugCounters* countersToRead)
        : Panel (moduleToControl.getName()), module (moduleToControl), counters (countersToRead)
    {
        setHelpId (*this, HelpId::panelSynth);

        const auto& presets = SynthModule::presets();
        for (size_t i = 0; i < presets.size(); ++i)
        {
            auto button = std::make_unique<juce::TextButton> (presets[i].name);
            setHelpId (*button, presets[i].id);
            button->onClick = [this, index = static_cast<int> (i)]
            {
                module.applyPreset (index);
                for (auto& knob : knobs)
                    knob->sync();
            };
            addAndMakeVisible (*button);
            presetButtons.push_back (std::move (button));
        }

        voices.setJustification (juce::Justification::centredRight);
        setHelpId (voices, "synth_voices");
        addAndMakeVisible (voices);

        for (const auto* p : module.getParameters())
        {
            knobs.push_back (std::make_unique<KnobControl> (registry.require (p->info().id)));
            addAndMakeVisible (*knobs.back());
        }

        graph = std::make_unique<EnvelopeGraph> (registry.require ("synth_attack"), registry.require ("synth_decay"),
                                                 registry.require ("synth_sustain"), registry.require ("synth_release"));
        setHelpId (*graph, "synth_envelope");
        addAndMakeVisible (*graph);

        timerCallback();
        startTimer (100);
    }

    ~SynthPanel() override { stopTimer(); }

    void resized() override
    {
        const auto& l = layout();
        auto area = getContentArea();

        auto top = area.removeFromTop (l.rowHeight);
        const int voicesWidth = l.labelWidth + l.panelGap * 2;
        voices.setBounds (top.removeFromRight (voicesWidth));
        top.removeFromRight (l.controlGap);

        const int count = static_cast<int> (presetButtons.size());
        const int buttonWidth = (top.getWidth() - l.controlGap * (count - 1)) / juce::jmax (1, count);
        for (auto& button : presetButtons)
        {
            button->setBounds (top.removeFromLeft (buttonWidth));
            top.removeFromLeft (l.controlGap);
        }

        area.removeFromTop (l.panelGap);

        auto knobArea = area.removeFromBottom (KnobControl::preferredHeight());
        area.removeFromBottom (l.panelGap);
        graph->setBounds (area);

        juce::FlexBox box;
        box.flexDirection = juce::FlexBox::Direction::row;
        box.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
        for (auto& knob : knobs)
            box.items.add (juce::FlexItem (*knob).withWidth (static_cast<float> (KnobControl::preferredWidth()))
                               .withHeight (static_cast<float> (KnobControl::preferredHeight())));
        box.performLayout (knobArea);
    }

private:
    void timerCallback() override
    {
        voices.setText ("Stemmer: " + juce::String ((counters != nullptr ? counters->activeVoices.load() : 0)) + " / "
                        + juce::String (SynthModule::numVoices));
    }

    SynthModule& module;
    DebugCounters* counters;
    std::vector<std::unique_ptr<juce::TextButton>> presetButtons;
    std::vector<std::unique_ptr<KnobControl>> knobs;
    std::unique_ptr<EnvelopeGraph> graph;
    CaptionLabel voices { {}, true };
};
} // namespace

std::unique_ptr<juce::Component> createSynthPanel (SynthModule& module, ParameterRegistry& registry, DebugCounters* counters)
{
    return std::make_unique<SynthPanel> (module, registry, counters);
}
} // namespace kb
