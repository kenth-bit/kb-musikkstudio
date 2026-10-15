#pragma once

#include "midi/MidiHub.h"
#include "modules/StudioModule.h"
#include "synth/Envelope.h"
#include "synth/SawOscillator.h"

#include <array>
#include <cstdint>
#include <vector>

namespace kb
{
struct SynthPreset
{
    juce::String id;
    juce::String name;
    juce::String help;
    float attack, decay, sustain, release, detune;
};

/** 80-talls synth: to sagtann-oscillatorer per stemme (med detune), 8 stemmer og ADSR.

    Regler som hindrer hengende toner:
    - Hver tone har høyst én stemme, så Note Off alltid treffer riktig stemme.
    - Stemmetyveri kan avbrytes: kommer Note Off før den nye tonen har startet, droppes den.
    - Et sikkerhetsnett sammenligner stemmene med tangenttilstanden i MidiState (hvilke tangenter
      som FAKTISK er nede) i hver lydblokk. En tapt Note Off kan derfor aldri bli en hengende tone.
    - En stemme i Release har en øvre grense for hvor lenge den kan klinge. */
class SynthModule : public StudioModule
{
public:
    static constexpr int numVoices = 8;

    SynthModule (ParameterRegistry& registry, DebugCounters* counters = nullptr, const MidiState* keySource = nullptr);

    juce::String getId() const override { return "synth"; }
    juce::String getName() const override { return "80-talls synth"; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void process (float* left, float* right, int numSamples, const MidiEvent* events, int numEvents) noexcept override;
    void allNotesOff() noexcept override;

    int getActiveVoices() const noexcept override;
    std::vector<const Parameter*> getParameters() const override;
    std::unique_ptr<juce::Component> createPanel() override;

    /** Antall stemmer der tangenten regnes som nede (gate). For tester og feilsøking. */
    int getNumGatedVoices() const noexcept;

    /** Klinger denne tonen, eller venter den på å starte etter stemmetyveri? For tester og feilsøking. */
    bool isNoteActive (int note) const noexcept;

    /** Antall stemmer som holdes oppe av sustain-pedalen. For tester og feilsøking. */
    int getNumSustainedVoices() const noexcept;

    /** Stille når ingen stemmer lyder og ingen venter på å starte. */
    bool isSilent() const noexcept { return getActiveVoices() == 0; }

    ParameterRegistry& getRegistry() noexcept { return registry; }

    void applyPreset (int index);
    static const std::vector<SynthPreset>& presets();

private:
    struct Voice
    {
        Envelope env;
        SawOscillator osc1, osc2;
        int note = -1;
        float velocityGain = 0.0f;
        bool gate = false;        // tangenten holdes nede
        bool sustained = false;   // tangenten er sluppet, men pedalen holder tonen
        std::uint64_t age = 0;
        double releaseSamples = 0.0;

        bool pendingValid = false;
        int pendingNote = -1;
        float pendingVelocity = 0.0f;

        bool isActive() const noexcept { return ! env.isIdle() || pendingValid; }
    };

    void readParameters() noexcept;
    void handleEvent (const MidiEvent& event) noexcept;
    void noteOn (int note, int velocity) noexcept;
    void noteOff (int note) noexcept;
    void setSustain (bool down) noexcept;
    void watchdog() noexcept;
    void renderVoices (float* left, float* right, int start, int end) noexcept;

    Voice* findVoiceForNote (int note) noexcept;
    Voice* findFreeVoice() noexcept;
    Voice* chooseVictim() noexcept;
    void startVoice (Voice& voice, int note, float velocityGain, bool continueFromCurrentLevel) noexcept;
    void releaseVoice (Voice& voice) noexcept;
    void applyPending (Voice& voice) noexcept;

    bool isKeyDown (int note) const noexcept;
    bool isPedalDown() const noexcept;
    std::uint32_t nextRandom() noexcept;

    ParameterRegistry& registry;
    DebugCounters* counters;
    const MidiState* keySource;

    Parameter& volumeParam;
    Parameter& detuneParam;
    Parameter& attackParam;
    Parameter& decayParam;
    Parameter& sustainParam;
    Parameter& releaseParam;

    double sampleRate = 48000.0;
    EnvelopeCoefficients envelopeCoefficients;
    double ratioUp = 1.0;
    double ratioDown = 1.0;
    float releaseSeconds = 1.0f;
    juce::SmoothedValue<float> volumeSmoothed;

    std::array<Voice, numVoices> voices;
    std::array<double, 128> noteHz {};
    std::array<bool, 128> keyDown {};
    bool pedalDown = false;
    std::uint64_t ageCounter = 0;
    std::uint32_t rngState = 0x9e3779b9u;
};
} // namespace kb
