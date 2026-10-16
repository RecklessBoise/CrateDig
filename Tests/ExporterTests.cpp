#include "Core/Exporter.h"

#include <juce_audio_formats/juce_audio_formats.h>

using namespace cratedig;

class ExporterTests final : public juce::UnitTest
{
public:
    ExporterTests() : juce::UnitTest ("Exporter", "CrateDig") {}

    void runTest() override
    {
        beginTest ("File names are sanitised");
        {
            expectEquals (exporter::sanitiseFileName ("AC/DC: \"Live\"  <1979>?"), juce::String ("ACDC Live 1979"));
            expectEquals (exporter::sanitiseFileName ("..hidden"), juce::String ("hidden"));
            expectEquals (exporter::sanitiseFileName ("   "), juce::String ("sample"));
            expectEquals (exporter::sanitiseFileName (juce::String::repeatedString ("a", 300)).length(), 120);
        }

        beginTest ("Base name includes artist, title, year, BPM and key");
        {
            SampleInfo info;
            info.identifier = "id";
            info.title = "Mississippi Mildred";
            info.creator = "Jelly-Roll Morton, Henry Allen";
            info.year = 1930;

            expectEquals (exporter::makeBaseName (info, 118.6, "Bb"),
                          juce::String ("Jelly-Roll Morton - Mississippi Mildred (1930) [119 BPM, Bb]"));

            info.creator = {};
            info.year = 0;
            expectEquals (exporter::makeBaseName (info, 0.0, {}), juce::String ("Mississippi Mildred"));
        }

        beginTest ("Region suffix only for partial regions");
        {
            expect (exporter::regionSuffix ({ 0, 44100 }, 44100, 44100.0).isEmpty());
            expectEquals (exporter::regionSuffix ({ 22050, 44100 }, 88200, 44100.0), juce::String (" (chop 0.5s-1.0s)"));
        }

        beginTest ("Exports a 24-bit WAV of the region and reuses it");
        {
            auto folder = juce::File::createTempFile ("cratedig-test");
            folder.createDirectory();

            juce::AudioBuffer<float> audio (2, 44100);

            for (int i = 0; i < audio.getNumSamples(); ++i)
            {
                audio.setSample (0, i, std::sin ((float) i * 0.01f) * 0.5f);
                audio.setSample (1, i, std::cos ((float) i * 0.01f) * 0.5f);
            }

            juce::String error;
            auto file = exporter::exportWav (audio, 44100.0, { 1000, 23050 }, folder, "Test Sample", error);
            expect (error.isEmpty(), error);
            expect (file.existsAsFile());
            expect (file.getFileName().startsWith ("Test Sample (chop"));

            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (file.createInputStream().release(), true));
            expect (reader != nullptr);

            if (reader != nullptr)
            {
                expectEquals ((int) reader->lengthInSamples, 22050);
                expectEquals ((int) reader->numChannels, 2);
                expectEquals ((int) reader->bitsPerSample, 24);
                expectEquals (reader->sampleRate, 44100.0);

                juce::AudioBuffer<float> readBack (2, 10);
                reader->read (&readBack, 0, 10, 0, true, true);
                expectWithinAbsoluteError (readBack.getSample (0, 0), audio.getSample (0, 1000), 1.0e-5f);
            }

            reader.reset();
            auto modified = file.getLastModificationTime();
            auto again = exporter::exportWav (audio, 44100.0, { 1000, 23050 }, folder, "Test Sample", error);
            expect (again == file);
            expect (again.getLastModificationTime() == modified);

            auto empty = exporter::exportWav (juce::AudioBuffer<float>(), 44100.0, {}, folder, "x", error);
            expect (! empty.exists());
            expect (error.isNotEmpty());

            folder.deleteRecursively();
        }
    }
};

static ExporterTests exporterTests;
