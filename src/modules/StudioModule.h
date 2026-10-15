#pragma once

#include "core/ParameterRegistry.h"
#include "midi/MidiEvent.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace kb
{
/** Alt som lager eller behandler lyd er en modul med dette grensesnittet.
    Å legge til en trommemaskin eller en effekt senere betyr: lag en ny modul og registrer den
    i ModuleRegistry. Kjernen trenger ikke endres. Se docs/LEGGE_TIL_MODUL.md. */
class StudioModule
{
public:
    virtual ~StudioModule() = default;

    virtual juce::String getId() const = 0;
    virtual juce::String getName() const = 0;

    /** Gjør klar. Her får modulen lov til å allokere minne. Kalles aldri fra lydtråden. */
    virtual void prepare (double sampleRate, int maxBlockSize) = 0;

    /** Lager lyd. Kjører på lydtråden: ingen allokering, ingen låser, ingen logging.
        Skriver (erstatter) utgangen i left/right. */
    virtual void process (float* left, float* right, int numSamples,
                          const MidiEvent* events, int numEvents) noexcept = 0;

    /** Slår av alle toner raskt (Panic). Trygt å kalle fra lydtråden. */
    virtual void allNotesOff() noexcept = 0;

    virtual int getActiveVoices() const noexcept { return 0; }

    /** Knottene til modulen (id, norsk navn, område, standardverdi og hjelpetekst). */
    virtual std::vector<const Parameter*> getParameters() const = 0;

    /** Modulens eget panel i vinduet. */
    virtual std::unique_ptr<juce::Component> createPanel() = 0;
};
} // namespace kb
