#include "audio/AudioEngine.h"
#include "core/Text.h"

#include "audio/Dsp.h"
#include "core/Logger.h"

#include <algorithm>
#include <cmath>

namespace kb
{
namespace
{
constexpr double retryIntervalSeconds = 2.0;
constexpr double fallbackProbeIntervalSeconds = 10.0;
constexpr int attemptsBeforeFallback = 6;
constexpr int staleTicksBeforeLost = 4;
const char* const windowsAudioName = "Windows Audio";

float masterGainFromDb (float db) noexcept
{
    return db <= -59.9f ? 0.0f : dbToGain (db);
}
} // namespace

AudioEngine::AudioEngine (MidiHub& hubToUse, Settings& settingsToUse, ParameterRegistry& parametersToUse)
    : hub (hubToUse),
      settings (settingsToUse),
      parameters (parametersToUse),
      masterParam (parametersToUse.require ("master"))
{
}

AudioEngine::~AudioEngine()
{
    shutdown();
}

double AudioEngine::nowSeconds()
{
    return juce::Time::getMillisecondCounterHiRes() * 0.001;
}

void AudioEngine::addModule (std::unique_ptr<StudioModule> module)
{
    jassert (! started);
    modules.push_back (std::move (module));
}

// ---------------------------------------------------------------------------------------
// Oppstart og avslutning

void AudioEngine::start()
{
    deviceManager.initialise (0, 2, nullptr, false);
    deviceManager.addAudioCallback (this);
    started = true;

    logDevices();
    loadPreferred();

    attemptReconnect();
    startTimer (500);
}

void AudioEngine::shutdown()
{
    stopTimer();

    if (started)
    {
        deviceManager.removeAudioCallback (this);
        deviceManager.closeAudioDevice();
        started = false;
    }
}

void AudioEngine::logDevices()
{
    bool foundFocusrite = false;
    bool haveAsio = false;
    juce::String report = "Lyddrivere og lydenheter:";

    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        type->scanForDevices();
        report << "\n  [" << type->getTypeName() << "]";

        const auto names = type->getDeviceNames (false);
        if (names.isEmpty())
            report << "\n    (ingen enheter)";

        for (const auto& name : names)
        {
            report << "\n    - " << name;
            if (name.containsIgnoreCase ("focusrite"))
                foundFocusrite = true;
        }

        if (type->getTypeName() == "ASIO")
            haveAsio = true;
    }

    Logger::get().info (report);

    if (foundFocusrite)
        Logger::get().info ("Focusrite-lydkort ble funnet.");
    else if (haveAsio)
        Logger::get().warn ("Fant ikke Focusrite USB ASIO. Sjekk at Scarlett er koblet til med USB og at Focusrite-driveren er installert.");
    else
        Logger::get().warn ("ASIO er ikke tilgjengelig. Bruker Windows Audio hvis det finnes.");
}

void AudioEngine::loadPreferred()
{
    preferred.driver = settings.getString ("audio.driver");
    preferred.output = settings.getString ("audio.output");
    preferred.sampleRate = settings.getDouble ("audio.sampleRate", 48000.0);
    preferred.bufferSize = settings.getInt ("audio.bufferSize", 256);

    if (preferred.driver.isEmpty() || preferred.output.isEmpty())
        choosePreferredDefaults();

    if (preferred.sampleRate != 44100.0 && preferred.sampleRate != 48000.0)
        preferred.sampleRate = 48000.0;
    if (preferred.bufferSize != 64 && preferred.bufferSize != 128 && preferred.bufferSize != 256
        && preferred.bufferSize != 512)
        preferred.bufferSize = 256;
}

void AudioEngine::choosePreferredDefaults()
{
    preferred.sampleRate = 48000.0;
    preferred.bufferSize = 256;

    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        if (type->getTypeName() != "ASIO")
            continue;

        for (const auto& name : type->getDeviceNames (false))
        {
            if (name.containsIgnoreCase ("focusrite"))
            {
                preferred.driver = type->getTypeName();
                preferred.output = name;
                return;
            }
        }
    }

    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        if (type->getTypeName() == windowsAudioName || preferred.driver.isEmpty())
        {
            const auto names = type->getDeviceNames (false);
            const int index = type->getDefaultDeviceIndex (false);

            if (names.size() > 0)
            {
                preferred.driver = type->getTypeName();
                preferred.output = names[juce::jlimit (0, names.size() - 1, index)];
                if (type->getTypeName() == windowsAudioName)
                    return;
            }
        }
    }
}

void AudioEngine::savePreferred()
{
    settings.setString ("audio.driver", preferred.driver);
    settings.setString ("audio.output", preferred.output);
    settings.setDouble ("audio.sampleRate", preferred.sampleRate);
    settings.setInt ("audio.bufferSize", preferred.bufferSize);
    settings.save();
}

// ---------------------------------------------------------------------------------------
// Enheter

juce::StringArray AudioEngine::getDriverNames() const
{
    juce::StringArray names;
    for (auto* type : const_cast<juce::AudioDeviceManager&> (deviceManager).getAvailableDeviceTypes())
        names.add (type->getTypeName());
    return names;
}

juce::StringArray AudioEngine::getOutputDeviceNames() const
{
    for (auto* type : const_cast<juce::AudioDeviceManager&> (deviceManager).getAvailableDeviceTypes())
        if (type->getTypeName() == preferred.driver)
            return type->getDeviceNames (false);

    return {};
}

juce::Array<double> AudioEngine::getAvailableSampleRates() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getAvailableSampleRates();
    return {};
}

juce::Array<int> AudioEngine::getAvailableBufferSizes() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getAvailableBufferSizes();
    return {};
}

double AudioEngine::getCurrentSampleRate() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentSampleRate();
    return preferred.sampleRate;
}

int AudioEngine::getCurrentBufferSize() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentBufferSizeSamples();
    return preferred.bufferSize;
}

double AudioEngine::getBufferLatencyMs() const
{
    const double rate = getCurrentSampleRate();
    return rate > 0.0 ? 1000.0 * getCurrentBufferSize() / rate : 0.0;
}

float AudioEngine::getCpuUsage() const
{
    return static_cast<float> (deviceManager.getCpuUsage());
}

bool AudioEngine::isDeviceOpen() const
{
    auto* device = deviceManager.getCurrentAudioDevice();
    return device != nullptr && device->isOpen();
}

bool AudioEngine::isDeviceAlive()
{
    auto* device = deviceManager.getCurrentAudioDevice();
    if (device == nullptr || ! device->isOpen() || ! device->isPlaying())
        return false;

    // Hvis ingen lydblokker har kommet på en stund, er driveren død selv om den later som den lever.
    const auto beats = heartbeat.load (std::memory_order_relaxed);
    if (beats == lastHeartbeat)
        return ++staleTicks < staleTicksBeforeLost;

    staleTicks = 0;
    lastHeartbeat = beats;
    return true;
}

juce::String AudioEngine::applyPreferred()
{
    deviceManager.closeAudioDevice();

    for (auto* type : deviceManager.getAvailableDeviceTypes())
        type->scanForDevices();

    bool driverExists = false;
    for (auto* type : deviceManager.getAvailableDeviceTypes())
        if (type->getTypeName() == preferred.driver)
            driverExists = true;

    if (! driverExists)
        return "Lyddriveren \"" + preferred.driver + "\" finnes ikke på denne PC-en."_nb;

    deviceManager.setCurrentAudioDeviceType (preferred.driver, true);

    auto* type = deviceManager.getCurrentDeviceTypeObject();
    if (type == nullptr || type->getTypeName() != preferred.driver)
        return "Kunne ikke velge lyddriveren \"" + preferred.driver + "\".";

    if (! type->getDeviceNames (false).contains (preferred.output))
        return "Fant ikke lydenheten \"" + preferred.output + "\".";

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup (setup);
    setup.outputDeviceName = preferred.output;
    setup.inputDeviceName = {};
    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();
    setup.useDefaultOutputChannels = true;
    setup.sampleRate = preferred.sampleRate;
    setup.bufferSize = preferred.bufferSize;

    auto error = deviceManager.setAudioDeviceSetup (setup, true);

    if (error.isEmpty() && ! isDeviceOpen())
        error = "Lydenheten kunne ikke åpnes."_nb;

    return error;
}

bool AudioEngine::openPreferred()
{
    const auto error = applyPreferred();

    if (error.isNotEmpty())
    {
        juce::String message = "Lyd: " + error;
        if (preferred.driver == "ASIO")
            message << " Hvis ASIO ikke vil åpne, kan et annet lydprogram (for eksempel en DAW) bruke Scarlett. Lukk det og trykk Koble til på nytt."_nb;
        Logger::get().warn (message);
        return false;
    }

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        preferred.sampleRate = device->getCurrentSampleRate();
        preferred.bufferSize = device->getCurrentBufferSizeSamples();
    }

    return true;
}

bool AudioEngine::openFallback()
{
    deviceManager.closeAudioDevice();

    juce::AudioIODeviceType* fallbackType = nullptr;
    for (auto* type : deviceManager.getAvailableDeviceTypes())
        if (type->getTypeName() == windowsAudioName)
            fallbackType = type;

    if (fallbackType == nullptr)
    {
        fallbackActive = false;
        return false;
    }

    fallbackType->scanForDevices();
    deviceManager.setCurrentAudioDeviceType (fallbackType->getTypeName(), true);

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup (setup);
    const auto names = fallbackType->getDeviceNames (false);

    if (names.isEmpty())
    {
        fallbackActive = false;
        return false;
    }

    setup.outputDeviceName = names[juce::jlimit (0, names.size() - 1, fallbackType->getDefaultDeviceIndex (false))];
    setup.inputDeviceName = {};
    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();
    setup.useDefaultOutputChannels = true;
    setup.sampleRate = 0.0;
    setup.bufferSize = 0;

    const auto error = deviceManager.setAudioDeviceSetup (setup, true);
    fallbackActive = error.isEmpty() && isDeviceOpen();

    if (fallbackActive)
    {
        staleTicks = 0;
        lastHeartbeat = heartbeat.load();
        Logger::get().warn ("Lyd: bruker Windows Audio som reserve mens jeg leter etter \"" + preferred.output
                            + "\". Forsinkelsen er større enn med ASIO."_nb);
    }

    return fallbackActive;
}

void AudioEngine::handleLoss (const juce::String& reason)
{
    Logger::get().warn ("Lyd: " + reason);

    testTone.setEnabled (false);
    deviceManager.closeAudioDevice();

    for (auto& module : modules)
        module->allNotesOff();

    fallbackActive = false;
    failedAttempts = 0;
    staleTicks = 0;
    tracker.onLost (nowSeconds());
}

void AudioEngine::attemptReconnect()
{
    tracker.onRetryStarted (nowSeconds());

    bool ok = false;

    if (fallbackActive)
    {
        ok = openPreferred();
        if (! ok)
            openFallback();
    }
    else
    {
        ok = openPreferred();

        if (! ok)
        {
            ++failedAttempts;
            if (failedAttempts >= attemptsBeforeFallback && preferred.driver != windowsAudioName)
                openFallback();
        }
    }

    const double now = nowSeconds();

    if (ok)
    {
        const bool wasDown = ! tracker.isConnected();
        tracker.onConnected (now);
        fallbackActive = false;
        failedAttempts = 0;
        staleTicks = 0;
        lastHeartbeat = heartbeat.load();
        savePreferred();

        if (wasDown)
            Logger::get().info ("Lyd: " + preferred.output + " er tilkoblet. Borte i "
                                + juce::String (tracker.lastOutageSeconds(), 1) + " sekunder.");
    }
    else
    {
        tracker.onRetryFailed (now);
    }
}

void AudioEngine::timerCallback()
{
    if (errorFlag.exchange (false))
    {
        if (tracker.isConnected())
        {
            handleLoss ("lydmotoren meldte en feil. Leter etter lydenheten igjen.");
            return;
        }
    }

    if (tracker.isConnected())
    {
        if (! isDeviceAlive())
            handleLoss ("lydenheten \"" + preferred.output + "\" ble borte. Er USB-kabelen til Scarlett løs? Leter etter den igjen."_nb);
        return;
    }

    const double interval = fallbackActive ? fallbackProbeIntervalSeconds : retryIntervalSeconds;
    if (tracker.shouldRetry (nowSeconds(), interval))
        attemptReconnect();
}

bool AudioEngine::applyUserChoice()
{
    const bool wasConnected = tracker.isConnected();

    if (openPreferred())
    {
        const double now = nowSeconds();
        tracker.onConnected (now);
        fallbackActive = false;
        failedAttempts = 0;
        staleTicks = 0;
        lastHeartbeat = heartbeat.load();
        savePreferred();
        Logger::get().info ("Lyd: " + preferred.driver + " / " + preferred.output + ", "
                            + juce::String (preferred.sampleRate, 0) + " Hz, buffer "
                            + juce::String (preferred.bufferSize));
        return true;
    }

    if (wasConnected)
        tracker.onLost (nowSeconds());

    return false;
}

void AudioEngine::selectDriver (const juce::String& driverName)
{
    preferred.driver = driverName;

    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        if (type->getTypeName() != driverName)
            continue;

        type->scanForDevices();
        const auto names = type->getDeviceNames (false);
        preferred.output = names.isEmpty() ? juce::String()
                                           : names[juce::jlimit (0, names.size() - 1, type->getDefaultDeviceIndex (false))];

        for (const auto& name : names)
            if (name.containsIgnoreCase ("focusrite"))
                preferred.output = name;
    }

    applyUserChoice();
}

void AudioEngine::selectOutput (const juce::String& deviceName)
{
    preferred.output = deviceName;
    applyUserChoice();
}

void AudioEngine::selectSampleRate (double sampleRate)
{
    preferred.sampleRate = sampleRate;
    applyUserChoice();
}

void AudioEngine::selectBufferSize (int bufferSize)
{
    preferred.bufferSize = bufferSize;
    applyUserChoice();
}

void AudioEngine::reconnectNow()
{
    if (tracker.isConnected())
        applyUserChoice();
    else
        attemptReconnect();
}

juce::String AudioEngine::getStatusText() const
{
    switch (tracker.state())
    {
        case ConnectionState::Connected:
            return "Lyd kjører: "_nb + preferred.output + " (" + preferred.driver + "), "
                   + juce::String (getCurrentSampleRate() / 1000.0, 1) + " kHz, buffer "
                   + juce::String (getCurrentBufferSize());

        case ConnectionState::Retrying:
            if (fallbackActive)
                return "Reserve: Windows Audio. Leter etter " + preferred.output;
            return "Prøver på nytt (forsøk "_nb + juce::String (tracker.attempts()) + ")";

        case ConnectionState::Lost:
            break;
    }

    if (preferred.output.isEmpty())
        return "Ingen lydenhet valgt";

    return "Lydenheten er borte: " + preferred.output;
}

// ---------------------------------------------------------------------------------------
// Lydtråden

void AudioEngine::prepareForTesting (double sampleRate, int blockSize)
{
    prepared.store (false, std::memory_order_release);

    currentSampleRate.store (sampleRate);
    const int size = std::max (blockSize, 1);
    mixLeft.assign (static_cast<size_t> (maxBlockSize), 0.0f);
    mixRight.assign (static_cast<size_t> (maxBlockSize), 0.0f);
    moduleLeft.assign (static_cast<size_t> (maxBlockSize), 0.0f);
    moduleRight.assign (static_cast<size_t> (maxBlockSize), 0.0f);
    eventScratch.assign (static_cast<size_t> (maxEventsPerBlock), MidiEvent());

    masterSmoothed.reset (sampleRate, 0.02);
    masterSmoothed.setCurrentAndTargetValue (masterGainFromDb (masterParam.get()));
    testTone.prepare (sampleRate);
    meter.reset();

    for (auto& module : modules)
        module->prepare (sampleRate, std::max (size, maxBlockSize));

    prepared.store (true, std::memory_order_release);
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepareForTesting (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void AudioEngine::audioDeviceStopped()
{
    prepared.store (false, std::memory_order_release);
}

void AudioEngine::audioDeviceError (const juce::String& errorMessage)
{
    Logger::get().error ("Lyd: feil fra lyddriveren: " + errorMessage);
    errorFlag.store (true, std::memory_order_release);
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                    float* const* outputChannelData, int numOutputChannels,
                                                    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    juce::ScopedNoDenormals noDenormals;
    heartbeat.fetch_add (1, std::memory_order_relaxed);

    for (int c = 0; c < numOutputChannels; ++c)
        if (outputChannelData[c] != nullptr)
            std::fill (outputChannelData[c], outputChannelData[c] + numSamples, 0.0f);

    if (! prepared.load (std::memory_order_acquire) || numOutputChannels < 1 || outputChannelData[0] == nullptr)
        return;

    if (numOutputChannels >= 2 && outputChannelData[1] != nullptr)
    {
        renderForTesting (outputChannelData[0], outputChannelData[1], numSamples);
        return;
    }

    // Enhet med én kanal: miks ned til mono.
    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (numSamples - done, maxBlockSize);
        renderForTesting (mixLeft.data(), mixRight.data(), n);
        for (int i = 0; i < n; ++i)
            outputChannelData[0][done + i] = 0.5f * (mixLeft[static_cast<size_t> (i)] + mixRight[static_cast<size_t> (i)]);
        done += n;
    }
}

void AudioEngine::renderForTesting (float* left, float* right, int numSamples) noexcept
{
    if (panicRequested.exchange (false, std::memory_order_acq_rel))
    {
        for (auto& module : modules)
            module->allNotesOff();
        testTone.setEnabled (false);
    }

    const int numEvents = hub.fifo.pop (eventScratch.data(), maxEventsPerBlock);

    int done = 0;
    bool first = true;

    while (done < numSamples)
    {
        const int n = std::min (numSamples - done, maxBlockSize);
        renderChunk (left + done, right + done, n, first ? eventScratch.data() : nullptr, first ? numEvents : 0);
        first = false;
        done += n;
    }
}

void AudioEngine::renderChunk (float* left, float* right, int numSamples,
                               const MidiEvent* events, int numEvents) noexcept
{
    std::fill (left, left + numSamples, 0.0f);
    std::fill (right, right + numSamples, 0.0f);

    bool eventsDelivered = false;
    int activeVoices = 0;

    for (auto& module : modules)
    {
        module->process (moduleLeft.data(), moduleRight.data(), numSamples, events, numEvents);
        eventsDelivered = true;
        activeVoices += module->getActiveVoices();

        for (int i = 0; i < numSamples; ++i)
        {
            left[i] += moduleLeft[static_cast<size_t> (i)];
            right[i] += moduleRight[static_cast<size_t> (i)];
        }
    }

    juce::ignoreUnused (eventsDelivered);
    hub.counters.activeVoices.store (activeVoices, std::memory_order_relaxed);

    testTone.addTo (left, right, numSamples);

    masterSmoothed.setTargetValue (masterGainFromDb (masterParam.get()));

    bool clipped = false;
    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = masterSmoothed.getNextValue();
        const float l = left[i] * gain;
        const float r = right[i] * gain;

        if (! (std::fabs (l) <= 1.0f) || ! (std::fabs (r) <= 1.0f))
            clipped = true;

        left[i] = SoftLimiter::process (l);
        right[i] = SoftLimiter::process (r);
    }

    if (clipped)
        meter.setClipped();

    meter.processBlock (left, right, numSamples, currentSampleRate.load (std::memory_order_relaxed));
}
} // namespace kb
