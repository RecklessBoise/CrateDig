// Renders the plugin editor to PNG files (used for README screenshots and layout checks).
// Usage: CrateDigSnapshot <output-folder> [--dig [path/to/yt-dlp]]

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
        cratedig::youtube::DigFilters filters;
        filters.genre = "Jazz Funk";
        filters.decade = 1970;
        processor->setFilters (filters);

        if (auto digIndex = args.indexOf ("--dig"); digIndex >= 0)
        {
            if (args[digIndex + 1].isNotEmpty())
                processor->setCustomYtDlp (juce::File (args[digIndex + 1].unquoted()));

            processor->search();
            stage = Stage::searching;
        }

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

        if (processor->isBusy() && elapsed < 120000)
            return;

        if (stage == Stage::searching)
        {
            std::cout << processor->getStatus() << std::endl;
            processor->listen();
            stage = Stage::listening;
            return;
        }

        if (editor == nullptr)
        {
            std::cout << processor->getStatus() << std::endl;

            if (processor->isSelectedLoaded())
            {
                processor->stop();
                auto total = processor->getLoaded().audio->getNumSamples();
                processor->setSelection ({ total / 5, total / 5 + total / 12 });
                processor->setStatus ("Previewing - Download or drag the chop to keep it");
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
    enum class Stage { idle, searching, listening };
    Stage stage = Stage::idle;
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
