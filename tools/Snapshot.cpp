// Renders the plugin editor to PNG files (used for README screenshots and layout checks).
// Usage: CrateDigSnapshot <output-folder> [--dig]

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <iostream>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

class SnapshotApp final : public juce::JUCEApplication,
                          private juce::Timer
{
public:
    const juce::String getApplicationName() override { return "CrateDigSnapshot"; }
    const juce::String getApplicationVersion() override { return CRATEDIG_VERSION; }

    void initialise (const juce::String& commandLine) override
    {
        auto args = juce::StringArray::fromTokens (commandLine, true);
        args.trim();
        args.removeEmptyStrings();

        folder = juce::File::getCurrentWorkingDirectory().getChildFile (args.isEmpty() ? "screenshots" : args[0].unquoted());
        folder.createDirectory();

        processor.reset (static_cast<CrateDigProcessor*> (createPluginFilter()));
        cratedig::archive::DigFilters filters;
        filters.genre = "Jazz";
        filters.decade = 1920;
        processor->setFilters (filters);

        if (args.contains ("--dig"))
            processor->dig();

        startedAt = juce::Time::getMillisecondCounter();
        startTimer (200);
    }

    void shutdown() override
    {
        editor.reset();
        processor.reset();
    }

private:
    void timerCallback() override
    {
        auto elapsed = juce::Time::getMillisecondCounter() - startedAt;

        if (editor == nullptr)
        {
            if (processor->isBusy() && elapsed < 60000)
                return;

            std::cout << processor->getStatus() << std::endl;

            if (processor->hasSample())
            {
                auto total = processor->getCurrent().audio->getNumSamples();
                processor->setSelection ({ total / 5, total / 5 + total / 12 });
                processor->setStatus ("Found it - drag the chop into your DAW");
            }

            editor.reset (processor->createEditor());
            return;
        }

        static const std::tuple<const char*, int, int> sizes[] { { "small", 765, 500 }, { "medium", 980, 640 }, { "large", 1274, 832 } };

        if (shot >= (int) std::size (sizes))
        {
            stopTimer();
            quit();
            return;
        }

        auto [name, w, h] = sizes[shot++];
        editor->setSize (w, h);
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        auto file = folder.getChildFile (juce::String ("cratedig-") + name + ".png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::cout << "Wrote " << file.getFullPathName() << std::endl;
    }

    juce::File folder;
    std::unique_ptr<CrateDigProcessor> processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::uint32 startedAt = 0;
    int shot = 0;
};

// START_JUCE_APPLICATION refuses to compile with the plugin's definitions, so wire main() by hand.
static juce::JUCEApplicationBase* createSnapshotApp() { return new SnapshotApp(); }

int main (int argc, char* argv[])
{
    juce::JUCEApplicationBase::createInstance = &createSnapshotApp;
    return juce::JUCEApplicationBase::main (argc, (const char**) argv);
}
