// Network smoke test: searches YouTube with yt-dlp, fetches the first result's preview,
// analyses it and exports a 4-second WAV chop into the temp folder.
// Usage: CrateDigSmoke [path/to/yt-dlp] [search text]

#include "Core/Digger.h"
#include "Core/Exporter.h"

#include <iostream>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;
    cratedig::Digger digger;
    int exitCode = 1;

    if (argc > 1)
        digger.setCustomYtDlp (juce::File (juce::String (argv[1])));

    cratedig::youtube::DigFilters filters;
    filters.text = argc > 2 ? juce::String (argv[2]) : juce::String();

    auto stop = [] { juce::MessageManager::getInstance()->stopDispatchLoop(); };

    digger.onStatus = [] (const juce::String& s) { std::cout << "  status: " << s << std::endl; };
    digger.onError = [&] (const juce::String& e)
    {
        std::cout << "ERROR: " << e << std::endl;
        stop();
    };
    std::vector<cratedig::SampleInfo> pending;

    digger.onResults = [&] (const juce::String& query, const std::vector<cratedig::SampleInfo>& results)
    {
        std::cout << results.size() << " results for \"" << query << "\"" << std::endl;

        for (size_t i = 0; i < juce::jmin<size_t> (5, results.size()); ++i)
            std::cout << "  - " << results[i].title << " | " << results[i].channel << " | " << results[i].durationSeconds << "s" << std::endl;

        pending = results;
    };

    digger.onLoaded = [&] (const cratedig::LoadedSample& s)
    {
        std::cout << "Loaded: " << s.info.title << " (" << s.audio->getNumSamples() / s.sampleRate << "s @ "
                  << s.sampleRate << " Hz), bpm " << s.bpm << ", key " << s.key << std::endl;

        juce::String error;
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("CrateDigSmoke");
        auto file = cratedig::exporter::exportWav (*s.audio, s.sampleRate, { 0, (int) (s.sampleRate * 4) }, folder,
                                                   cratedig::exporter::makeBaseName (s.info, s.bpm, s.key), error);
        std::cout << (file.existsAsFile() ? "Exported " + file.getFullPathName() : "Export failed: " + error) << std::endl;
        exitCode = file.existsAsFile() ? 0 : 1;
        stop();
    };

    // onResults fires just before the search thread exits, so the fetch starts once the digger is idle.
    struct FetchWhenIdle final : juce::Timer
    {
        std::function<void()> tick;
        void timerCallback() override { tick(); }
    } poller;

    poller.tick = [&]
    {
        if (! pending.empty() && ! digger.isBusy())
        {
            digger.fetch (pending.front());
            pending.clear();
        }
    };
    poller.startTimer (100);

    digger.search (filters);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (240000);
    return exitCode;
}
