#pragma once

#include "audio/AudioEngine.h"
#include "ui/Components.h"

namespace kb
{
/** Master-fader, nivåmålere, klipp-lampe, testtone og Panic. */
class MasterPanel : public Panel,
                    private juce::Timer
{
public:
    MasterPanel (AudioEngine& audioEngine, ParameterRegistry& parameters);
    ~MasterPanel() override;

    void resized() override;
    void lookAndFeelChanged() override;

private:
    class MeterScale;
    class FaderValue;

    void timerCallback() override;
    void syncFader();
    void applyButtonColours();

    AudioEngine& audio;
    Parameter& masterParam;

    juce::Slider fader;
    std::unique_ptr<FaderValue> faderValue;
    std::unique_ptr<MeterScale> scale;
    LevelMeterView meterLeft;
    LevelMeterView meterRight;
    ClipLamp clipLamp;
    CaptionLabel clipLabel { "KLIPP" };
    juce::TextButton testToneButton { "Testtone" };
    juce::TextButton panicButton { "Panic" };
};
} // namespace kb
