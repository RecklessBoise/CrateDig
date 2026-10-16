#include "Core/Analysis.h"

#include <cmath>

using namespace cratedig;

namespace
{
    constexpr double sr = 44100.0;

    juce::AudioBuffer<float> clickTrack (double bpm, double seconds)
    {
        juce::AudioBuffer<float> buffer (1, (int) (sr * seconds));
        buffer.clear();
        auto interval = sr * 60.0 / bpm;
        juce::Random rng (42);

        for (double pos = 0.0; pos < buffer.getNumSamples(); pos += interval)
            for (int i = 0; i < 800 && (int) pos + i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, (int) pos + i, (rng.nextFloat() * 2.0f - 1.0f) * std::exp ((float) -i / 150.0f));

        return buffer;
    }

    juce::AudioBuffer<float> chord (std::initializer_list<double> midiNotes, double seconds)
    {
        juce::AudioBuffer<float> buffer (2, (int) (sr * seconds));
        buffer.clear();

        for (auto note : midiNotes)
        {
            auto freq = 440.0 * std::pow (2.0, (note - 69.0) / 12.0);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                auto v = (float) (0.2 * std::sin (juce::MathConstants<double>::twoPi * freq * i / sr));
                buffer.addSample (0, i, v);
                buffer.addSample (1, i, v);
            }
        }

        return buffer;
    }
} // namespace

class AnalysisTests final : public juce::UnitTest
{
public:
    AnalysisTests() : juce::UnitTest ("Analysis", "CrateDig") {}

    void runTest() override
    {
        beginTest ("BPM of a click track");
        {
            for (auto bpm : { 90.0, 120.0, 140.0 })
            {
                auto estimate = analysis::estimateBpm (clickTrack (bpm, 20.0), sr);
                logMessage ("  " + juce::String (bpm) + " -> " + juce::String (estimate));
                expectWithinAbsoluteError (estimate, bpm, 2.0);
            }
        }

        beginTest ("Double-time readings are folded below 160 BPM");
        {
            auto estimate = analysis::estimateBpm (clickTrack (176.0, 20.0), sr);
            expectWithinAbsoluteError (estimate, 88.0, 2.0);
        }

        beginTest ("BPM of silence is zero");
        {
            juce::AudioBuffer<float> silence (1, (int) sr * 5);
            silence.clear();
            expectEquals (analysis::estimateBpm (silence, sr), 0.0);
            expectEquals (analysis::estimateBpm (silence, 0.0), 0.0);
        }

        beginTest ("Key of a major triad");
        {
            expectEquals (analysis::estimateKey (chord ({ 48, 60, 64, 67 }, 4.0), sr), juce::String ("C"));
            expectEquals (analysis::estimateKey (chord ({ 43, 55, 59, 62 }, 4.0), sr), juce::String ("G"));
        }

        beginTest ("Key of a minor triad");
        {
            expectEquals (analysis::estimateKey (chord ({ 45, 57, 60, 64 }, 4.0), sr), juce::String ("Am"));
        }

        beginTest ("Key of silence is empty");
        {
            juce::AudioBuffer<float> silence (1, (int) sr * 2);
            silence.clear();
            expect (analysis::estimateKey (silence, sr).isEmpty());
        }

        beginTest ("Normalise to -1 dBFS");
        {
            auto buffer = chord ({ 60 }, 1.0);
            analysis::normalise (buffer, -1.0f);
            expectWithinAbsoluteError (buffer.getMagnitude (0, buffer.getNumSamples()),
                                       juce::Decibels::decibelsToGain (-1.0f), 1.0e-4f);
        }
    }
};

static AnalysisTests analysisTests;
