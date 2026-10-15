#include "core/HelpTexts.h"
#include "core/Text.h"

namespace kb
{
void registerCoreParameters (ParameterRegistry& registry)
{
    if (registry.find (HelpId::master) == nullptr)
    {
        registry.addParameter ({ HelpId::master, "Master", "Master",
            "Master: total lyd ut av programmet. Start lavt og skru opp etter at Scarlett og hodetelefonene sitter.",
            -60.0f, 6.0f, -12.0f, 1.0f, ParamFormat::Decibel });
    }

    auto add = [&registry] (const char* id, const juce::String& name, const juce::String& group, const juce::String& help)
    {
        if (registry.getHelp (id).isEmpty())
            registry.addControlHelp ({ id, name, group, help });
    };

    add (HelpId::audioDriver, "Lyddriver", "Tilkobling",
         "Lyddriver: hvilken type lydkommunikasjon programmet bruker. Velg ASIO når du bruker Scarlett, det gir minst forsinkelse. Windows Audio virker uten driver, men har mer forsinkelse."_nb);
    add (HelpId::audioOutput, "Lydutgang", "Tilkobling",
         "Lydutgang: hvor lyden sendes. Velg Focusrite USB ASIO for Scarlett Solo.");
    add (HelpId::audioSampleRate, "Sample rate", "Tilkobling",
         "Sample rate: hvor mange lydmålinger det tas per sekund. 44.1 kHz er CD-kvalitet, 48 kHz er standard i film og studio. Den bør være lik innstillingen i Focusrite Control."_nb);
    add (HelpId::audioBuffer, "Buffer", "Tilkobling",
         "Buffer: hvor mange lydprøver som samles før de sendes ut. Lavere tall gir mindre forsinkelse, men tyngre jobb for PC-en. Start på 256. Går det uten knitring, prøv 128 og 64."_nb);
    add (HelpId::audioLatency, "Forsinkelse", "Tilkobling",
         "Forsinkelse: tiden fra du trykker en tangent til lyden går ut, regnet fra bufferen. Under ca. 10 ms merkes den knapt."_nb);
    add (HelpId::audioStatus, "Lydstatus", "Tilkobling",
         "Lydstatus: grønn betyr at lyden kjører. Gul betyr at programmet prøver å koble til igjen eller bruker reservelyd. Rød betyr at lydenheten er borte."_nb);
    add (HelpId::audioReconnect, "Koble til på nytt"_nb, "Tilkobling",
         "Koble til på nytt: prøver å finne Scarlett igjen. Bruk den hvis stripen er rød etter at du har satt USB-kabelen tilbake."_nb);
    add (HelpId::midiDevice, "MIDI-enhet", "Tilkobling",
         "MIDI-enhet: hvilket piano programmet lytter på. Velg det 88-tangenters digitalpianoet."_nb);
    add (HelpId::midiStatus, "MIDI-status", "Tilkobling",
         "MIDI-status: grønn betyr at pianoet er koblet til. Gul betyr at programmet prøver å finne det igjen. Rød betyr at pianoet er borte."_nb);
    add (HelpId::midiRescan, "Skann på nytt"_nb, "Tilkobling",
         "Skann på nytt: leter etter MIDI-enheter igjen. Bruk den hvis pianoet ikke vises i listen."_nb);

    add (HelpId::meter, "Måler"_nb, "Master",
         "Måler: viser hvor sterk lyden er ut. Den fylte stolpen er gjennomsnittsnivået, den lyse streken er toppnivået."_nb);
    add (HelpId::clip, "Klipp-lampe", "Master",
         "Klipp-lampe: lyser rødt hvis lyden gikk over 0 dB, altså var for høy. Skru ned Master. Klikk på lampen for å nullstille den."_nb);
    add (HelpId::testTone, "Testtone", "Master",
         "Testtone: spiller en jevn tone (440 Hz, tonen A) så du kan sjekke at lyden kommer ut av Scarlett. Skru volumet lavt først."_nb);
    add (HelpId::panic, "Panic", "Master",
         "Panic: slår av alle toner med en gang. Bruk den hvis en tone henger seg opp."_nb);
    add (HelpId::cpu, "CPU", "Topplinje",
         "CPU: hvor travelt lydmotoren har det. Går det over 70 prosent, eller hører du knitring, så øk bufferen."_nb);

    add (HelpId::keyboard, "Tangenter", "Tangenter",
         "Tangenter: de 88 tangentene på pianoet. De lyser når du spiller, og lysstyrken viser hvor hardt du trykket. Midtre C kalles C4 og er MIDI-nummer 60."_nb);
    add (HelpId::noteInfo, "Sist spilte tone", "Tangenter",
         "Sist spilte tone: navn, MIDI-nummer og frekvens i Hz. Midtre C er C4, nummer 60 og 261.6 Hz.");
    add (HelpId::velocity, "Velocity", "Tangenter",
         "Velocity: hvor hardt du treffer tangenten, fra 0 til 127. Hardere anslag gir sterkere tone.");
    add (HelpId::sustainPedal, "Sustain-pedal", "Tangenter",
         "Sustain-pedal: hold-pedalen. Tonene klinger videre etter at du slipper tangentene, helt til du slipper pedalen.");
    add (HelpId::pitchBend, "Pitch bend", "Tangenter",
         "Pitch bend: hjulet som bøyer tonen opp og ned. Vises bare foreløpig, det styrer ingen lyd ennå."_nb);
    add (HelpId::modWheel, "Modulasjonshjul", "Tangenter",
         "Modulasjonshjul: hjulet som gir lyden mer uttrykk. Vises bare foreløpig, det styrer ingen lyd ennå."_nb);

    add (HelpId::log, "Logg", "Logg",
         "Logg: meldinger fra programmet, for eksempel om pianoet eller Scarlett ble koblet fra.");
    add (HelpId::logDebug, "Feilsøking"_nb, "Logg",
         "Feilsøking: viser tellere for tangent ned og tangent opp som er mottatt og behandlet, hendelser som gikk tapt i køen, og aktive stemmer. Tallene for mottatt og behandlet skal stemme med hverandre."_nb);

    add (HelpId::panelConnection, "Panelet Tilkobling", "Vinduet",
         "Tilkobling: her velger du pianoet (MIDI) og lydkortet (Scarlett), og ser om begge er koblet til.");
    add (HelpId::panelSynth, "Panelet Synth", "Vinduet",
         "Synth: her former du 80-talls lyden. Velg en startlyd øverst og drei på knottene for å endre den."_nb);
    add (HelpId::panelMaster, "Panelet Master", "Vinduet",
         "Master: total lydstyrke, nivåmåler, testtone og Panic."_nb);
    add (HelpId::panelKeys, "Panelet Tangenter", "Vinduet",
         "Tangenter: viser pianoet ditt på skjermen og hva du spiller."_nb);
    add (HelpId::panelLog, "Panelet Logg", "Vinduet",
         "Logg: meldinger fra programmet og feilsøking."_nb);
}
} // namespace kb
