#include "synth/SynthModule.h"
#include "core/Text.h"

#include "midi/NoteMath.h"
#include "ui/SynthPanel.h"

#include <algorithm>
#include <cmath>

namespace kb
{
namespace
{
constexpr float voiceScale = 0.15f;

Parameter& getOrAdd (ParameterRegistry& registry, ParamInfo info)
{
    if (auto* existing = registry.find (info.id))
        return *existing;
    return registry.addParameter (std::move (info));
}
} // namespace

const std::vector<SynthPreset>& SynthModule::presets()
{
    static const std::vector<SynthPreset> list {
        { "preset_pad", "Juno-pad",
          "Varm 80-talls pad: mykt anslag og lang utklang, med litt bredde. Fin til akkorder med høyre hånd."_nb,
          0.45f, 0.9f, 0.8f, 1.4f, 16.0f },
        { "preset_strings", "Strings",
          "Strykere: tonen kommer rolig og holder jevnt. God til lange akkorder og flater.",
          0.25f, 0.6f, 0.85f, 1.0f, 10.0f },
        { "preset_pluck", "Pluck",
          "Kort og perkussiv: tonen smeller inn og dør raskt ut. Fin til arpeggioer og melodier."_nb,
          0.002f, 0.35f, 0.0f, 0.25f, 8.0f },
        { "preset_bass", "Bass",
          "Fyldig basslyd: rask start, litt dekk og kort utklang. Spill i venstre hånd, lavt på pianoet."_nb,
          0.003f, 0.4f, 0.5f, 0.12f, 4.0f }
    };
    return list;
}

SynthModule::SynthModule (ParameterRegistry& registryToUse, DebugCounters* countersToUse, const MidiState* keySourceToUse)
    : registry (registryToUse),
      counters (countersToUse),
      keySource (keySourceToUse),
      volumeParam (getOrAdd (registryToUse, { "synth_volume", "Volum", "Synth",
          "Volum: nivået på synthen før master. Start rundt 70 prosent og juster med Master."_nb,
          0.0f, 1.0f, 0.7f, 1.0f, ParamFormat::Percent })),
      detuneParam (getOrAdd (registryToUse, { "synth_detune", "Detune", "Synth",
          "Detune: hvor mye de to sagtannlagene stemmes fra hverandre. Mer detune gir bredere og fetere lyd.",
          0.0f, 50.0f, 16.0f, 1.0f, ParamFormat::Cents })),
      attackParam (getOrAdd (registryToUse, { "synth_attack", "Attack", "Synth",
          "Attack: hvor raskt tonen kommer til full styrke når du trykker. Kort = skarp. Lang = myk pad."_nb,
          0.001f, 5.0f, 0.45f, 0.3f, ParamFormat::Time })),
      decayParam (getOrAdd (registryToUse, { "synth_decay", "Decay", "Synth",
          "Decay: hvor raskt tonen faller ned til sustain-nivået etter at den har nådd full styrke."_nb,
          0.001f, 5.0f, 0.9f, 0.3f, ParamFormat::Time })),
      sustainParam (getOrAdd (registryToUse, { "synth_sustain", "Sustain", "Synth",
          "Sustain: nivået tonen holder mens du holder tangenten nede. Dette er et nivå, ikke en tid."_nb,
          0.0f, 1.0f, 0.8f, 1.0f, ParamFormat::Percent })),
      releaseParam (getOrAdd (registryToUse, { "synth_release", "Release", "Synth",
          "Release: hvor lenge tonen klinger ut etter at du slipper tangenten.",
          0.005f, 8.0f, 1.4f, 0.3f, ParamFormat::Time }))
{
    for (const auto& preset : presets())
        if (registry.getHelp (preset.id).isEmpty())
            registry.addControlHelp ({ preset.id, preset.name, "Synth", preset.help });

    if (registry.getHelp ("synth_voices").isEmpty())
        registry.addControlHelp ({ "synth_voices", "Stemmer", "Synth",
                                   "Stemmer: hvor mange toner synthen spiller akkurat nå, av åtte mulige. Spiller du flere, tar synthen over den eldste tonen."_nb });

    if (registry.getHelp ("synth_envelope").isEmpty())
        registry.addControlHelp ({ "synth_envelope", "Kurve", "Synth",
                                   "Kurve: viser hvordan volumet til en tone utvikler seg fra du trykker til du slipper (Attack, Decay, Sustain, Release)." });

    for (int note = 0; note < 128; ++note)
        noteHz[static_cast<size_t> (note)] = noteToHz (note);
}

std::vector<const Parameter*> SynthModule::getParameters() const
{
    return { &volumeParam, &detuneParam, &attackParam, &decayParam, &sustainParam, &releaseParam };
}

std::unique_ptr<juce::Component> SynthModule::createPanel()
{
    return createSynthPanel (*this, registry, counters);
}

void SynthModule::applyPreset (int index)
{
    const auto& list = presets();
    if (index < 0 || index >= static_cast<int> (list.size()))
        return;

    const auto& p = list[static_cast<size_t> (index)];
    attackParam.set (p.attack);
    decayParam.set (p.decay);
    sustainParam.set (p.sustain);
    releaseParam.set (p.release);
    detuneParam.set (p.detune);
}

void SynthModule::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

    for (auto& v : voices)
    {
        v = Voice();
        v.env.reset();
    }

    keyDown.fill (false);
    pedalDown = false;
    ageCounter = 0;

    volumeSmoothed.reset (sampleRate, 0.02);
    volumeSmoothed.setCurrentAndTargetValue (volumeParam.get());
    readParameters();
}

void SynthModule::readParameters() noexcept
{
    releaseSeconds = releaseParam.get();
    envelopeCoefficients = EnvelopeCoefficients::make (sampleRate, attackParam.get(), decayParam.get(),
                                                       sustainParam.get(), releaseSeconds);

    const double cents = static_cast<double> (detuneParam.get());
    ratioUp = std::exp2 (cents * 0.5 / 1200.0);
    ratioDown = std::exp2 (-cents * 0.5 / 1200.0);

    volumeSmoothed.setTargetValue (volumeParam.get());

    for (auto& v : voices)
    {
        v.env.setCoefficients (envelopeCoefficients);
        if (v.note >= 0 && ! v.env.isIdle())
        {
            const double hz = noteHz[static_cast<size_t> (v.note)];
            v.osc1.setFrequency (hz * ratioUp, sampleRate);
            v.osc2.setFrequency (hz * ratioDown, sampleRate);
        }
    }
}

bool SynthModule::isKeyDown (int note) const noexcept
{
    if (note < 0 || note > 127)
        return false;
    return keySource != nullptr ? keySource->isDown (note) : keyDown[static_cast<size_t> (note)];
}

bool SynthModule::isPedalDown() const noexcept
{
    return keySource != nullptr ? keySource->isSustainDown() : pedalDown;
}

std::uint32_t SynthModule::nextRandom() noexcept
{
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}

int SynthModule::getActiveVoices() const noexcept
{
    int count = 0;
    for (const auto& v : voices)
        if (v.isActive())
            ++count;
    return count;
}

int SynthModule::getNumGatedVoices() const noexcept
{
    int count = 0;
    for (const auto& v : voices)
        if (v.gate && ! v.env.isIdle())
            ++count;
    return count;
}

bool SynthModule::isNoteActive (int note) const noexcept
{
    for (const auto& v : voices)
    {
        if (v.pendingValid && v.pendingNote == note)
            return true;
        if (v.note == note && ! v.env.isIdle())
            return true;
    }
    return false;
}

int SynthModule::getNumSustainedVoices() const noexcept
{
    int count = 0;
    for (const auto& v : voices)
        if (v.sustained && ! v.env.isIdle())
            ++count;
    return count;
}

// ---------------------------------------------------------------------------------------
// Stemmehåndtering

void SynthModule::releaseVoice (Voice& voice) noexcept
{
    voice.gate = false;
    voice.sustained = false;
    voice.releaseSamples = 0.0;
    voice.env.noteOff();
}

void SynthModule::startVoice (Voice& voice, int note, float velocityGain, bool continueFromCurrentLevel) noexcept
{
    const bool sameNoteSounding = continueFromCurrentLevel && ! voice.env.isIdle() && voice.note == note;

    voice.note = note;
    voice.velocityGain = velocityGain;
    voice.gate = true;
    voice.sustained = false;
    voice.releaseSamples = 0.0;
    voice.age = ++ageCounter;
    voice.pendingValid = false;

    const double hz = noteHz[static_cast<size_t> (note)];
    voice.osc1.setFrequency (hz * ratioUp, sampleRate);
    voice.osc2.setFrequency (hz * ratioDown, sampleRate);

    if (! sameNoteSounding)
    {
        voice.osc1.setPhase (static_cast<double> (nextRandom() & 0xFFFFu) / 65536.0);
        voice.osc2.setPhase (static_cast<double> (nextRandom() & 0xFFFFu) / 65536.0);
        voice.env.reset();
    }

    voice.env.setCoefficients (envelopeCoefficients);
    voice.env.noteOn();
}

void SynthModule::applyPending (Voice& voice) noexcept
{
    const int note = voice.pendingNote;
    const float velocity = voice.pendingVelocity;
    voice.pendingValid = false;
    startVoice (voice, note, velocity, false);
}

SynthModule::Voice* SynthModule::findVoiceForNote (int note) noexcept
{
    for (auto& v : voices)
        if (v.note == note && ! v.env.isIdle() && ! v.pendingValid && (v.gate || v.sustained || v.env.getStage() != Envelope::Stage::FastRelease))
            return &v;
    return nullptr;
}

SynthModule::Voice* SynthModule::findFreeVoice() noexcept
{
    for (auto& v : voices)
        if (! v.isActive())
            return &v;
    return nullptr;
}

SynthModule::Voice* SynthModule::chooseVictim() noexcept
{
    Voice* releasing = nullptr;
    Voice* sustained = nullptr;
    Voice* held = nullptr;
    Voice* any = nullptr;

    for (auto& v : voices)
    {
        if (any == nullptr || v.age < any->age)
            any = &v;

        if (v.pendingValid)
            continue;

        if (! v.gate && ! v.sustained)
        {
            if (releasing == nullptr || v.env.getLevel() < releasing->env.getLevel())
                releasing = &v;
        }
        else if (v.sustained)
        {
            if (sustained == nullptr || v.age < sustained->age)
                sustained = &v;
        }
        else if (held == nullptr || v.age < held->age)
        {
            held = &v;
        }
    }

    if (releasing != nullptr) return releasing;
    if (sustained != nullptr) return sustained;
    if (held != nullptr)      return held;
    return any;
}

void SynthModule::noteOn (int note, int velocity) noexcept
{
    if (note < 0 || note > 127)
        return;

    keyDown[static_cast<size_t> (note)] = true;
    const float gain = velocityToGain (velocity);

    // En tone som venter på å starte (etter stemmetyveri): bare oppdater anslaget.
    for (auto& v : voices)
    {
        if (v.pendingValid && v.pendingNote == note)
        {
            v.pendingVelocity = gain;
            return;
        }
    }

    // Samme tone som allerede klinger: bruk samme stemme og start på nytt uten klikk.
    if (auto* same = findVoiceForNote (note))
    {
        startVoice (*same, note, gain, true);
        return;
    }

    if (auto* freeVoice = findFreeVoice())
    {
        startVoice (*freeVoice, note, gain, false);
        return;
    }

    // Alle stemmer er i bruk: ta over en, med rask nedfading så det ikke klikker.
    if (auto* victim = chooseVictim())
    {
        victim->gate = false;
        victim->sustained = false;
        victim->pendingValid = true;
        victim->pendingNote = note;
        victim->pendingVelocity = gain;
        victim->env.fastRelease();
    }
}

void SynthModule::noteOff (int note) noexcept
{
    if (note < 0 || note > 127)
        return;

    keyDown[static_cast<size_t> (note)] = false;

    for (auto& v : voices)
    {
        if (v.pendingValid && v.pendingNote == note)
            v.pendingValid = false;

        if (v.gate && v.note == note)
        {
            if (pedalDown)
            {
                v.gate = false;
                v.sustained = true;
            }
            else
            {
                releaseVoice (v);
            }
        }
    }
}

void SynthModule::setSustain (bool down) noexcept
{
    pedalDown = down;

    if (down)
        return;

    for (auto& v : voices)
        if (v.sustained)
            releaseVoice (v);
}

void SynthModule::allNotesOff() noexcept
{
    keyDown.fill (false);
    pedalDown = false;

    for (auto& v : voices)
    {
        v.gate = false;
        v.sustained = false;
        v.pendingValid = false;
        v.releaseSamples = 0.0;
        v.env.fastRelease();
    }
}

void SynthModule::handleEvent (const MidiEvent& event) noexcept
{
    switch (event.type)
    {
        case MidiEventType::NoteOn:
            if (counters != nullptr)
                counters->noteOnProcessed.fetch_add (1, std::memory_order_relaxed);
            if (event.velocity == 0)
                noteOff (event.note);
            else
                noteOn (event.note, event.velocity);
            break;

        case MidiEventType::NoteOff:
            if (counters != nullptr)
                counters->noteOffProcessed.fetch_add (1, std::memory_order_relaxed);
            noteOff (event.note);
            break;

        case MidiEventType::Sustain:
            setSustain (event.value >= 64);
            break;

        case MidiEventType::AllNotesOff:
            allNotesOff();
            break;

        case MidiEventType::ModWheel:
        case MidiEventType::PitchBend:
            break;
    }
}

void SynthModule::watchdog() noexcept
{
    const bool pedal = isPedalDown();

    for (auto& v : voices)
    {
        if (v.env.isIdle() && ! v.pendingValid)
        {
            v.gate = false;
            v.sustained = false;
            continue;
        }

        if (v.pendingValid && ! isKeyDown (v.pendingNote) && ! pedal)
            v.pendingValid = false;

        if (v.gate && ! isKeyDown (v.note))
        {
            if (pedal)
            {
                v.gate = false;
                v.sustained = true;
            }
            else
            {
                releaseVoice (v);
                if (counters != nullptr)
                    counters->watchdogReleases.fetch_add (1, std::memory_order_relaxed);
            }
        }

        if (v.sustained && ! pedal)
            releaseVoice (v);

        if (! v.gate && ! v.sustained && ! v.env.isIdle() && ! v.env.isReleasing())
            releaseVoice (v);
    }
}

// ---------------------------------------------------------------------------------------
// Lyd

void SynthModule::renderVoices (float* left, float* right, int start, int end) noexcept
{
    const int length = end - start;
    if (length <= 0)
        return;

    const double releaseLimit = (2.0 * static_cast<double> (releaseSeconds) + 0.1) * sampleRate;

    for (auto& v : voices)
    {
        if (! v.isActive())
            continue;

        for (int i = start; i < end; ++i)
        {
            if (v.env.isIdle())
            {
                if (! v.pendingValid)
                    break;
                applyPending (v);
            }

            const float level = v.env.next() * v.velocityGain * voiceScale;
            const float a = v.osc1.next();
            const float b = v.osc2.next();

            left[i] += level * (0.65f * a + 0.35f * b);
            right[i] += level * (0.35f * a + 0.65f * b);
        }

        if (v.env.isReleasing())
        {
            v.releaseSamples += length;
            if (v.env.getStage() == Envelope::Stage::Release && v.releaseSamples > releaseLimit)
                v.env.fastRelease();
        }
    }

    for (int i = start; i < end; ++i)
    {
        const float gain = volumeSmoothed.getNextValue();
        left[i] *= gain;
        right[i] *= gain;
    }
}

void SynthModule::process (float* left, float* right, int numSamples, const MidiEvent* events, int numEvents) noexcept
{
    std::fill (left, left + numSamples, 0.0f);
    std::fill (right, right + numSamples, 0.0f);

    readParameters();

    int position = 0;
    int eventIndex = 0;

    while (position < numSamples)
    {
        while (eventIndex < numEvents && events[eventIndex].sampleOffset <= position)
            handleEvent (events[eventIndex++]);

        watchdog();

        int segmentEnd = numSamples;
        if (eventIndex < numEvents)
            segmentEnd = std::clamp (events[eventIndex].sampleOffset, position + 1, numSamples);

        renderVoices (left, right, position, segmentEnd);
        position = segmentEnd;
    }

    while (eventIndex < numEvents)
        handleEvent (events[eventIndex++]);

    watchdog();

    if (counters != nullptr)
        counters->activeVoices.store (getActiveVoices(), std::memory_order_relaxed);
}
} // namespace kb
