#pragma once

#include <algorithm>
#include <cmath>

namespace kb
{
/** Koeffisienter for en ADSR-kurve. Regnes ut én gang per lydblokk og deles av alle stemmer. */
struct EnvelopeCoefficients
{
    float attackIncrement = 0.01f;   // nivåøkning per sample i Attack (lineær)
    float decayCoef = 0.999f;        // per sample, mot Sustain-nivået
    float releaseCoef = 0.999f;      // per sample i Release (til -80 dB)
    float fastCoef = 0.99f;          // ca. 5 ms ned i stillhet
    float sustainSmooth = 0.01f;     // glatter endring i Sustain-nivå
    float sustainLevel = 0.8f;

    /** Tider i sekunder. Null eller svært korte tider er trygge (får en nedre grense). */
    static EnvelopeCoefficients make (double sampleRate, float attack, float decay, float sustain, float release) noexcept
    {
        EnvelopeCoefficients c;
        const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        c.attackIncrement = static_cast<float> (1.0 / (std::max (static_cast<double> (attack), 0.001) * sr));
        c.decayCoef = static_cast<float> (std::exp (std::log (0.001) / (std::max (static_cast<double> (decay), 0.001) * sr)));
        c.releaseCoef = static_cast<float> (std::exp (std::log (0.0001) / (std::max (static_cast<double> (release), 0.005) * sr)));
        c.fastCoef = static_cast<float> (std::exp (std::log (0.001) / (0.005 * sr)));
        c.sustainSmooth = static_cast<float> (1.0 - std::exp (-1.0 / (0.005 * sr)));
        c.sustainLevel = std::clamp (sustain, 0.0f, 1.0f);
        return c;
    }
};

/** ADSR per stemme. Alle overganger er klikkfrie (nivået hopper aldri). */
class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release, FastRelease };

    void setCoefficients (const EnvelopeCoefficients& newCoefficients) noexcept { c = newCoefficients; }

    /** Starter (eller starter på nytt) fra nåværende nivå, så en gjentatt tone ikke klikker. */
    void noteOn() noexcept { stage = Stage::Attack; }

    void noteOff() noexcept
    {
        if (stage != Stage::Idle && stage != Stage::FastRelease)
            stage = Stage::Release;
    }

    /** Rask nedfading (ca. 5 ms) til stillhet. Brukes av Panic og stemmetyveri. */
    void fastRelease() noexcept
    {
        if (stage != Stage::Idle)
            stage = Stage::FastRelease;
    }

    void reset() noexcept
    {
        stage = Stage::Idle;
        level = 0.0f;
    }

    bool isIdle() const noexcept { return stage == Stage::Idle; }
    bool isReleasing() const noexcept { return stage == Stage::Release || stage == Stage::FastRelease; }
    Stage getStage() const noexcept { return stage; }
    float getLevel() const noexcept { return level; }

    float next() noexcept
    {
        switch (stage)
        {
            case Stage::Idle:
                return 0.0f;

            case Stage::Attack:
                level += c.attackIncrement;
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stage = Stage::Decay;
                }
                break;

            case Stage::Decay:
                level = c.sustainLevel + (level - c.sustainLevel) * c.decayCoef;
                if (std::fabs (level - c.sustainLevel) < 1.0e-4f)
                {
                    level = c.sustainLevel;
                    stage = Stage::Sustain;
                }
                break;

            case Stage::Sustain:
                level += (c.sustainLevel - level) * c.sustainSmooth;
                break;

            case Stage::Release:
                level *= c.releaseCoef;
                if (level < 1.0e-4f)
                    reset();
                break;

            case Stage::FastRelease:
                level *= c.fastCoef;
                if (level < 1.0e-4f)
                    reset();
                break;
        }

        return level;
    }

private:
    EnvelopeCoefficients c;
    Stage stage = Stage::Idle;
    float level = 0.0f;
};
} // namespace kb
