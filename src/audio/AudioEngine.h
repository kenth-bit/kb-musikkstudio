#pragma once

#include "audio/LevelMeter.h"
#include "audio/TestTone.h"
#include "core/ConnectionTracker.h"
#include "core/ParameterRegistry.h"
#include "core/Settings.h"
#include "midi/MidiHub.h"
#include "modules/StudioModule.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <memory>
#include <vector>

namespace kb
{
/** Lydmotoren. Eier lydenheten (ASIO/WASAPI), kjører lyd-callbacken, henter MIDI fra køen,
    sender den til modulene, og passer på master, måler, limiter og gjenkobling.

    Regler for lydtråden (callbacken): ingen allokering, ingen låser, ingen filaksess,
    ingen unntak og ingen logging. UI og lydtråd snakker kun via atomics og køen. */
class AudioEngine : public juce::AudioIODeviceCallback,
                    private juce::Timer
{
public:
    AudioEngine (MidiHub& hub, Settings& settings, ParameterRegistry& parameters);
    ~AudioEngine() override;

    /** Moduler må legges til før start(). */
    void addModule (std::unique_ptr<StudioModule> module);
    const std::vector<std::unique_ptr<StudioModule>>& getModules() const noexcept { return modules; }

    /** Lister enheter i loggen, åpner valgt (eller beste) enhet og begynner å passe på tilkoblingen. */
    void start();
    void shutdown();

    // --- Valg i brukergrensesnittet (meldingstråden) -------------------------------------
    juce::StringArray getDriverNames() const;
    juce::String getCurrentDriver() const { return preferred.driver; }
    juce::StringArray getOutputDeviceNames() const;
    juce::String getCurrentOutput() const { return preferred.output; }
    juce::Array<double> getAvailableSampleRates() const;
    juce::Array<int> getAvailableBufferSizes() const;
    double getCurrentSampleRate() const;
    int getCurrentBufferSize() const;
    double getBufferLatencyMs() const;

    void selectDriver (const juce::String& driverName);
    void selectOutput (const juce::String& deviceName);
    void selectSampleRate (double sampleRate);
    void selectBufferSize (int bufferSize);

    /** "Koble til på nytt": ett umiddelbart forsøk. */
    void reconnectNow();

    const ConnectionTracker& getTracker() const noexcept { return tracker; }
    juce::String getStatusText() const;
    bool isUsingFallback() const noexcept { return fallbackActive; }
    float getCpuUsage() const;

    // --- Master, måler, testtone, panic --------------------------------------------------
    LevelMeter& getMeter() noexcept { return meter; }
    void setTestToneEnabled (bool shouldPlay) noexcept { testTone.setEnabled (shouldPlay); }
    bool isTestToneEnabled() const noexcept { return testTone.isEnabled(); }

    /** Panic: slår av alle toner og testtonen, og slukker tangentlysene.
        Trygt å kalle fra UI-tråden. */
    void panic() noexcept
    {
        hub.state.releaseAll();
        panicRequested.store (true, std::memory_order_release);
    }

    // --- juce::AudioIODeviceCallback -----------------------------------------------------
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceError (const juce::String& errorMessage) override;

    /** Brukes av tester for å kjøre lydmotoren uten lydkort. */
    void renderForTesting (float* left, float* right, int numSamples) noexcept;
    void prepareForTesting (double sampleRate, int maxBlockSize);

    static constexpr int maxBlockSize = 4096;
    static constexpr int maxEventsPerBlock = 512;

private:
    struct Preferred
    {
        juce::String driver;
        juce::String output;
        double sampleRate = 48000.0;
        int bufferSize = 256;
    };

    void timerCallback() override;

    void loadPreferred();
    void choosePreferredDefaults();
    void savePreferred();
    void logDevices();

    juce::String applyPreferred();
    bool isDeviceOpen() const;
    bool isDeviceAlive();
    bool openPreferred();
    bool openFallback();
    bool applyUserChoice();
    void handleLoss (const juce::String& reason);
    void attemptReconnect();
    static double nowSeconds();

    void renderChunk (float* left, float* right, int numSamples, const MidiEvent* events, int numEvents) noexcept;

    MidiHub& hub;
    Settings& settings;
    ParameterRegistry& parameters;
    Parameter& masterParam;

    juce::AudioDeviceManager deviceManager;
    std::vector<std::unique_ptr<StudioModule>> modules;

    Preferred preferred;
    ConnectionTracker tracker;
    bool fallbackActive = false;
    bool started = false;
    int failedAttempts = 0;
    int staleTicks = 0;
    std::uint32_t lastHeartbeat = 0;

    // Delt med lydtråden
    std::atomic<bool> panicRequested { false };
    std::atomic<bool> errorFlag { false };
    std::atomic<std::uint32_t> heartbeat { 0 };
    std::atomic<bool> prepared { false };
    std::atomic<double> currentSampleRate { 48000.0 };

    // Bare lydtråden bruker disse etter prepare
    std::vector<float> mixLeft, mixRight, moduleLeft, moduleRight;
    std::vector<MidiEvent> eventScratch;
    juce::SmoothedValue<float> masterSmoothed;
    TestTone testTone;
    LevelMeter meter;
};
} // namespace kb
