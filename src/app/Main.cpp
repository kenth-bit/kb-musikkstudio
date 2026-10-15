#include "app/StudioContext.h"
#include "core/CrashHandler.h"
#include "core/Logger.h"
#include "core/Text.h"
#include "ui/Layout.h"
#include "ui/MainComponent.h"
#include "ui/Theme.h"
#include "ui/ThemeLoader.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace kb
{
namespace
{
struct Options
{
    juce::File screenshotFile;
    int width = 0;
    int height = 0;
    juce::String helpId = "synth_attack";

    bool isScreenshot() const { return screenshotFile != juce::File(); }
};

Options parseOptions (const juce::StringArray& args)
{
    Options options;

    for (int i = 0; i < args.size(); ++i)
    {
        if (args[i] == "--screenshot" && i + 1 < args.size())
            options.screenshotFile = juce::File::getCurrentWorkingDirectory().getChildFile (args[++i]);
        else if (args[i] == "--help-id" && i + 1 < args.size())
            options.helpId = args[++i];
        else if (args[i] == "--size" && i + 1 < args.size())
        {
            const auto parts = juce::StringArray::fromTokens (args[++i], "x", "");
            if (parts.size() == 2)
            {
                options.width = parts[0].getIntValue();
                options.height = parts[1].getIntValue();
            }
        }
    }

    return options;
}

/** Fyller vinduet med eksempeldata til skjermbilder: noen tangenter nede, nivå på målerne og litt logg. */
void seedDemo (StudioContext& context)
{
    context.hub.state.apply (MidiEvent::noteOn (55, 60));
    context.hub.state.apply (MidiEvent::noteOff (55));
    context.hub.counters.midiNoteOnReceived.fetch_add (1);
    context.hub.counters.noteOnProcessed.fetch_add (1);
    context.hub.counters.midiNoteOffReceived.fetch_add (1);
    context.hub.counters.noteOffProcessed.fetch_add (1);

    const int notes[][2] = { { 36, 100 }, { 60, 92 }, { 64, 70 }, { 67, 112 }, { 72, 44 }, { 79, 80 } };
    for (const auto& n : notes)
    {
        context.hub.state.apply (MidiEvent::noteOn (n[0], n[1]));
        context.hub.counters.midiNoteOnReceived.fetch_add (1);
        context.hub.counters.noteOnProcessed.fetch_add (1);
    }

    context.hub.state.apply (MidiEvent::sustain (true));
    context.hub.counters.activeVoices.store (6);

    constexpr int blockSize = 512;
    std::vector<float> left (blockSize), right (blockSize);
    for (int block = 0; block < 12; ++block)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            const float s = 0.42f * std::sin (0.07f * static_cast<float> (block * blockSize + i));
            left[static_cast<size_t> (i)] = s;
            right[static_cast<size_t> (i)] = 0.8f * s;
        }
        context.audio.getMeter().processBlock (left.data(), right.data(), blockSize, 48000.0);
    }

    Logger::get().info ("Eksempeldata for skjermbilde er lagt inn."_nb);
}

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow (StudioContext& context)
        : DocumentWindow ("KB Musikkstudio", theme().background, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, true);

        auto content = std::make_unique<MainComponent> (context);
        main = content.get();
        main->onThemeReloaded = [this] { applyLimits(); };
        setContentOwned (content.release(), true);

        applyLimits();
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
        main->grabKeyboardFocus();
    }

    MainComponent& getMain() { return *main; }

    void applyLimits()
    {
        const auto& l = layout();
        setResizeLimits (l.minWindowWidth, l.minWindowHeight, 10000, 10000);
        setBackgroundColour (theme().background);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    MainComponent* main = nullptr;
};
} // namespace

class KBApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "KB Musikkstudio"; }
    const juce::String getApplicationVersion() override { return KB_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return screenshotMode; }

    void initialise (const juce::String&) override
    {
        const auto options = parseOptions (getCommandLineParameterArray());
        screenshotMode = options.isScreenshot();

        auto dataDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("KBMusikkstudio");
        if (screenshotMode)
            dataDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("KBMusikkstudio-skjermbilde");

        dataDir.createDirectory();

        Logger::get().setLogFile (dataDir.getChildFile ("logg.txt"));
        installCrashHandler (dataDir.getChildFile ("krasjlogg.txt"));

        Logger::get().info ("KB Musikkstudio " + juce::String (KB_VERSION_STRING) + " starter.");
        Logger::get().info ("Innstillinger og logg ligger i: " + dataDir.getFullPathName());

        ThemeLoader::reload();

        context = std::make_unique<StudioContext> (dataDir.getChildFile ("innstillinger.json"));
        window = std::make_unique<MainWindow> (*context);

        if (options.width > 0 && options.height > 0)
            window->setContentComponentSize (options.width, options.height);

        context->start();

        if (screenshotMode)
        {
            seedDemo (*context);
            const auto file = options.screenshotFile;
            juce::Timer::callAfterDelay (1200, [this, file, helpId = options.helpId] { saveScreenshot (file, helpId); });
        }
    }

    void shutdown() override
    {
        window.reset();
        context.reset();
        Logger::get().info ("KB Musikkstudio er avsluttet.");
    }

    void systemRequestedQuit() override { quit(); }

    void anotherInstanceStarted (const juce::String&) override {}

    void unhandledException (const std::exception* e, const juce::String& file, int line) override
    {
        Logger::get().error ("Uventet feil: " + juce::String (e != nullptr ? e->what() : "ukjent") + " (" + file
                             + ":" + juce::String (line) + ")");
    }

private:
    void saveScreenshot (const juce::File& file, const juce::String& helpId)
    {
        file.getParentDirectory().createDirectory();
        auto& main = window->getMain();
        HelpHub::get().setText (context->parameters.getHelp (helpId));
        const auto image = main.createComponentSnapshot (main.getLocalBounds(), true, 1.0f);

        file.deleteFile();
        juce::FileOutputStream stream (file);
        bool ok = false;

        if (stream.openedOk())
        {
            juce::PNGImageFormat png;
            ok = png.writeImageToStream (image, stream);
        }

        Logger::get().info (ok ? "Skjermbilde lagret: " + file.getFullPathName()
                               : "Kunne ikke lagre skjermbilde: " + file.getFullPathName());
        quit();
    }

    bool screenshotMode = false;
    std::unique_ptr<StudioContext> context;
    std::unique_ptr<MainWindow> window;
};
} // namespace kb

START_JUCE_APPLICATION (kb::KBApplication)
