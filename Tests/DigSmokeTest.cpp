// Network smoke test: digs a real record on archive.org, decodes it, analyses it and exports a WAV.
// Usage: CrateDigSmoke [genre] [decade]

#include "Core/Digger.h"
#include "Core/Exporter.h"

#include <iostream>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;
    cratedig::Digger digger;
    int exitCode = 1;

    cratedig::archive::DigFilters filters;
    filters.genre = argc > 1 ? juce::String (argv[1]) : juce::String();
    filters.decade = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;

    digger.onStatus = [] (const juce::String& s) { std::cout << "  status: " << s << std::endl; };
    digger.onError = [&] (const juce::String& e)
    {
        std::cout << "ERROR: " << e << std::endl;
        juce::MessageManager::getInstance()->stopDispatchLoop();
    };
    digger.onLoaded = [&] (const cratedig::LoadedSample& s)
    {
        std::cout << "Found: " << s.info.creator << " - " << s.info.title << " (" << s.info.year << ")\n"
                  << "  genre " << s.info.genre() << ", " << s.audio->getNumSamples() / s.sampleRate << "s @ "
                  << s.sampleRate << " Hz, " << s.audio->getNumChannels() << " ch\n"
                  << "  bpm " << s.bpm << ", key " << s.key << "\n  " << s.info.pageUrl() << std::endl;

        juce::String error;
        auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("CrateDigSmoke");
        auto file = cratedig::exporter::exportWav (*s.audio, s.sampleRate, { 0, (int) (s.sampleRate * 4) }, folder,
                                                   cratedig::exporter::makeBaseName (s.info, s.bpm, s.key), error);
        std::cout << (file.existsAsFile() ? "Exported " + file.getFullPathName() : "Export failed: " + error) << std::endl;
        exitCode = file.existsAsFile() ? 0 : 1;
        juce::MessageManager::getInstance()->stopDispatchLoop();
    };

    digger.dig (filters);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (90000);
    return exitCode;
}
