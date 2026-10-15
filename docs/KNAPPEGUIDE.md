# Knappeguide

Her står det hva hver knapp, knott og lampe i KB Musikkstudio gjør. Samme tekster vises i hjelpelinjen nederst i programmet når du peker på en kontroll.

> Denne filen lages automatisk fra teksten i koden. Ikke rediger den for hånd. Lag den på nytt med `cmake --build build --target docs`.

## Tilkobling

### Lyddriver

Lyddriver: hvilken type lydkommunikasjon programmet bruker. Velg ASIO når du bruker Scarlett, det gir minst forsinkelse. Windows Audio virker uten driver, men har mer forsinkelse.

### Lydutgang

Lydutgang: hvor lyden sendes. Velg Focusrite USB ASIO for Scarlett Solo.

### Sample rate

Sample rate: hvor mange lydmålinger det tas per sekund. 44.1 kHz er CD-kvalitet, 48 kHz er standard i film og studio. Den bør være lik innstillingen i Focusrite Control.

### Buffer

Buffer: hvor mange lydprøver som samles før de sendes ut. Lavere tall gir mindre forsinkelse, men tyngre jobb for PC-en. Start på 256. Går det uten knitring, prøv 128 og 64.

### Forsinkelse

Forsinkelse: tiden fra du trykker en tangent til lyden går ut, regnet fra bufferen. Under ca. 10 ms merkes den knapt.

### Lydstatus

Lydstatus: grønn betyr at lyden kjører. Gul betyr at programmet prøver å koble til igjen eller bruker reservelyd. Rød betyr at lydenheten er borte.

### Koble til på nytt

Koble til på nytt: prøver å finne Scarlett igjen. Bruk den hvis stripen er rød etter at du har satt USB-kabelen tilbake.

### MIDI-enhet

MIDI-enhet: hvilket piano programmet lytter på. Velg det 88-tangenters digitalpianoet.

### MIDI-status

MIDI-status: grønn betyr at pianoet er koblet til. Gul betyr at programmet prøver å finne det igjen. Rød betyr at pianoet er borte.

### Skann på nytt

Skann på nytt: leter etter MIDI-enheter igjen. Bruk den hvis pianoet ikke vises i listen.

## Synth

### Volum

Volum: nivået på synthen før master. Start rundt 70 prosent og juster med Master.

*Område: 0 % til 100 %. Standard: 70 %. Dobbeltklikk på knotten for å nullstille.*

### Detune

Detune: hvor mye de to sagtannlagene stemmes fra hverandre. Mer detune gir bredere og fetere lyd.

*Område: 0 cent til 50 cent. Standard: 16 cent. Dobbeltklikk på knotten for å nullstille.*

### Attack

Attack: hvor raskt tonen kommer til full styrke når du trykker. Kort = skarp. Lang = myk pad.

*Område: 1 ms til 5.00 s. Standard: 450 ms. Dobbeltklikk på knotten for å nullstille.*

### Decay

Decay: hvor raskt tonen faller ned til sustain-nivået etter at den har nådd full styrke.

*Område: 1 ms til 5.00 s. Standard: 900 ms. Dobbeltklikk på knotten for å nullstille.*

### Sustain

Sustain: nivået tonen holder mens du holder tangenten nede. Dette er et nivå, ikke en tid.

*Område: 0 % til 100 %. Standard: 80 %. Dobbeltklikk på knotten for å nullstille.*

### Release

Release: hvor lenge tonen klinger ut etter at du slipper tangenten.

*Område: 5 ms til 8.00 s. Standard: 1.40 s. Dobbeltklikk på knotten for å nullstille.*

### Juno-pad

Varm 80-talls pad: mykt anslag og lang utklang, med litt bredde. Fin til akkorder med høyre hånd.

### Strings

Strykere: tonen kommer rolig og holder jevnt. God til lange akkorder og flater.

### Pluck

Kort og perkussiv: tonen smeller inn og dør raskt ut. Fin til arpeggioer og melodier.

### Bass

Fyldig basslyd: rask start, litt dekk og kort utklang. Spill i venstre hånd, lavt på pianoet.

### Stemmer

Stemmer: hvor mange toner synthen spiller akkurat nå, av åtte mulige. Spiller du flere, tar synthen over den eldste tonen.

### Kurve

Kurve: viser hvordan volumet til en tone utvikler seg fra du trykker til du slipper (Attack, Decay, Sustain, Release).

## Master

### Master

Master: total lyd ut av programmet. Start lavt og skru opp etter at Scarlett og hodetelefonene sitter.

*Område: -inf dB til 6.0 dB. Standard: -12.0 dB. Dobbeltklikk på knotten for å nullstille.*

### Måler

Måler: viser hvor sterk lyden er ut. Den fylte stolpen er gjennomsnittsnivået, den lyse streken er toppnivået.

### Klipp-lampe

Klipp-lampe: lyser rødt hvis lyden gikk over 0 dB, altså var for høy. Skru ned Master. Klikk på lampen for å nullstille den.

### Testtone

Testtone: spiller en jevn tone (440 Hz, tonen A) så du kan sjekke at lyden kommer ut av Scarlett. Skru volumet lavt først.

### Panic

Panic: slår av alle toner med en gang. Bruk den hvis en tone henger seg opp.

## Tangenter

### Tangenter

Tangenter: de 88 tangentene på pianoet. De lyser når du spiller, og lysstyrken viser hvor hardt du trykket. Midtre C kalles C4 og er MIDI-nummer 60.

### Sist spilte tone

Sist spilte tone: navn, MIDI-nummer og frekvens i Hz. Midtre C er C4, nummer 60 og 261.6 Hz.

### Velocity

Velocity: hvor hardt du treffer tangenten, fra 0 til 127. Hardere anslag gir sterkere tone.

### Sustain-pedal

Sustain-pedal: hold-pedalen. Tonene klinger videre etter at du slipper tangentene, helt til du slipper pedalen.

### Pitch bend

Pitch bend: hjulet som bøyer tonen opp og ned. Vises bare foreløpig, det styrer ingen lyd ennå.

### Modulasjonshjul

Modulasjonshjul: hjulet som gir lyden mer uttrykk. Vises bare foreløpig, det styrer ingen lyd ennå.

## Logg

### Logg

Logg: meldinger fra programmet, for eksempel om pianoet eller Scarlett ble koblet fra.

### Feilsøking

Feilsøking: viser tellere for tangent ned og tangent opp som er mottatt og behandlet, hendelser som gikk tapt i køen, og aktive stemmer. Tallene for mottatt og behandlet skal stemme med hverandre.

## Topplinje

### CPU

CPU: hvor travelt lydmotoren har det. Går det over 70 prosent, eller hører du knitring, så øk bufferen.

## Vinduet

### Panelet Tilkobling

Tilkobling: her velger du pianoet (MIDI) og lydkortet (Scarlett), og ser om begge er koblet til.

### Panelet Synth

Synth: her former du 80-talls lyden. Velg en startlyd øverst og drei på knottene for å endre den.

### Panelet Master

Master: total lydstyrke, nivåmåler, testtone og Panic.

### Panelet Tangenter

Tangenter: viser pianoet ditt på skjermen og hva du spiller.

### Panelet Logg

Logg: meldinger fra programmet og feilsøking.

