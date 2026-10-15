#pragma once

#include "core/ParameterRegistry.h"

namespace kb
{
/** Registrerer master-volumet og hjelpetekstene til alle kontroller som ikke er synth-knotter.
    Her ligger den eneste kilden til forklaringene i hjelpelinjen og i docs/KNAPPEGUIDE.md. */
void registerCoreParameters (ParameterRegistry& registry);

/** Navn på kontroller brukt av UI. Samme id-er som i HelpTexts.cpp. */
namespace HelpId
{
inline constexpr const char* audioDriver = "audio_driver";
inline constexpr const char* audioOutput = "audio_output";
inline constexpr const char* audioSampleRate = "audio_samplerate";
inline constexpr const char* audioBuffer = "audio_buffer";
inline constexpr const char* audioLatency = "audio_latency";
inline constexpr const char* audioStatus = "audio_status";
inline constexpr const char* audioReconnect = "audio_reconnect";
inline constexpr const char* midiDevice = "midi_device";
inline constexpr const char* midiStatus = "midi_status";
inline constexpr const char* midiRescan = "midi_rescan";
inline constexpr const char* master = "master";
inline constexpr const char* meter = "meter";
inline constexpr const char* clip = "clip";
inline constexpr const char* testTone = "testtone";
inline constexpr const char* panic = "panic";
inline constexpr const char* keyboard = "keyboard";
inline constexpr const char* noteInfo = "note_info";
inline constexpr const char* velocity = "velocity";
inline constexpr const char* sustainPedal = "sustain_pedal";
inline constexpr const char* pitchBend = "pitchbend";
inline constexpr const char* modWheel = "modwheel";
inline constexpr const char* log = "log";
inline constexpr const char* logDebug = "log_debug";
inline constexpr const char* cpu = "cpu";
inline constexpr const char* panelConnection = "panel_connection";
inline constexpr const char* panelSynth = "panel_synth";
inline constexpr const char* panelMaster = "panel_master";
inline constexpr const char* panelKeys = "panel_keys";
inline constexpr const char* panelLog = "panel_log";
} // namespace HelpId
} // namespace kb
