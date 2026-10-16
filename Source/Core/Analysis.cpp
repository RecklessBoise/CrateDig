#include "Analysis.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cmath>
#include <vector>

namespace cratedig::analysis
{

namespace
{
    constexpr double maxAnalysisSeconds = 90.0;

    std::vector<float> mixToMono (const juce::AudioBuffer<float>& audio, double sampleRate)
    {
        auto numSamples = juce::jmin (audio.getNumSamples(), (int) (sampleRate * maxAnalysisSeconds));
        std::vector<float> mono ((size_t) juce::jmax (0, numSamples), 0.0f);

        if (audio.getNumChannels() == 0)
            return mono;

        auto gain = 1.0f / (float) audio.getNumChannels();

        for (int ch = 0; ch < audio.getNumChannels(); ++ch)
        {
            auto* data = audio.getReadPointer (ch);

            for (int i = 0; i < numSamples; ++i)
                mono[(size_t) i] += data[i] * gain;
        }

        return mono;
    }
} // namespace

double estimateBpm (const juce::AudioBuffer<float>& audio, double sampleRate)
{
    if (sampleRate <= 0.0)
        return 0.0;

    auto mono = mixToMono (audio, sampleRate);
    constexpr int hop = 512;
    auto numFrames = (int) mono.size() / hop;

    if (numFrames < 64)
        return 0.0;

    // Log-energy per frame, then half-wave rectified difference = onset strength.
    std::vector<float> energy ((size_t) numFrames);

    for (int f = 0; f < numFrames; ++f)
    {
        double sum = 0.0;

        for (int i = 0; i < hop; ++i)
        {
            auto s = mono[(size_t) (f * hop + i)];
            sum += (double) s * s;
        }

        energy[(size_t) f] = (float) std::log1p (1000.0 * sum / hop);
    }

    std::vector<float> onset ((size_t) numFrames, 0.0f);
    float mean = 0.0f;

    for (int f = 1; f < numFrames; ++f)
    {
        onset[(size_t) f] = juce::jmax (0.0f, energy[(size_t) f] - energy[(size_t) f - 1]);
        mean += onset[(size_t) f];
    }

    mean /= (float) numFrames;

    if (mean <= 1.0e-6f)
        return 0.0;

    for (auto& o : onset)
        o -= mean;

    auto frameRate = sampleRate / hop;
    constexpr double minBpm = 60.0, maxBpm = 180.0;
    auto minLag = juce::jmax (1, (int) std::floor (frameRate * 60.0 / maxBpm));
    auto maxLag = juce::jmin (numFrames - 1, (int) std::ceil (frameRate * 60.0 / minBpm));

    auto bestScore = 0.0;
    auto bestLag = 0.0;

    std::vector<double> scores ((size_t) maxLag + 2, 0.0);

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double acf = 0.0;

        for (int f = lag; f < numFrames; ++f)
            acf += (double) onset[(size_t) f] * onset[(size_t) (f - lag)];

        acf /= (numFrames - lag);
        scores[(size_t) lag] = acf;

        // Mild preference for tempos around 115 BPM to resolve octave ambiguity.
        auto bpm = 60.0 * frameRate / lag;
        auto weight = std::exp (-0.5 * std::pow (std::log2 (bpm / 115.0) / 0.9, 2.0));
        auto score = acf * weight;

        if (score > bestScore)
        {
            bestScore = score;
            bestLag = lag;
        }
    }

    if (bestLag <= 0.0)
        return 0.0;

    // Parabolic interpolation around the peak for sub-frame precision.
    auto l = (int) bestLag;

    if (l > minLag && l < maxLag)
    {
        auto a = scores[(size_t) l - 1], b = scores[(size_t) l], c = scores[(size_t) l + 1];
        auto denom = a - 2.0 * b + c;

        if (std::abs (denom) > 1.0e-12)
            bestLag = l + juce::jlimit (-0.5, 0.5, 0.5 * (a - c) / denom);
    }

    auto bpm = 60.0 * frameRate / bestLag;

    // Old dance records are mostly 70-160 BPM; fold double-time readings back into that range.
    while (bpm > 160.0)
        bpm *= 0.5;

    return std::round (bpm * 10.0) / 10.0;
}

juce::String estimateKey (const juce::AudioBuffer<float>& audio, double sampleRate)
{
    if (sampleRate <= 0.0)
        return {};

    auto mono = mixToMono (audio, sampleRate);
    constexpr int order = 13;
    constexpr int fftSize = 1 << order;

    if ((int) mono.size() < fftSize)
        return {};

    juce::dsp::FFT fft (order);
    juce::dsp::WindowingFunction<float> window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false);
    std::vector<float> frame ((size_t) fftSize * 2);
    std::array<double, 12> chroma {};

    // Bin -> pitch class lookup for 55 Hz .. 2 kHz.
    std::vector<int> pitchClassForBin ((size_t) fftSize / 2, -1);

    for (int bin = 1; bin < fftSize / 2; ++bin)
    {
        auto freq = bin * sampleRate / fftSize;

        if (freq < 55.0 || freq > 2000.0)
            continue;

        auto midi = 69.0 + 12.0 * std::log2 (freq / 440.0);
        pitchClassForBin[(size_t) bin] = ((int) std::lround (midi) % 12 + 12) % 12;
    }

    for (size_t start = 0; start + fftSize <= mono.size(); start += fftSize / 2)
    {
        std::fill (frame.begin(), frame.end(), 0.0f);
        std::copy (mono.begin() + (long) start, mono.begin() + (long) start + fftSize, frame.begin());
        window.multiplyWithWindowingTable (frame.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (frame.data(), true);

        for (int bin = 1; bin < fftSize / 2; ++bin)
            if (auto pc = pitchClassForBin[(size_t) bin]; pc >= 0)
                chroma[(size_t) pc] += frame[(size_t) bin];
    }

    double total = 0.0;

    for (auto c : chroma)
        total += c;

    if (total <= 1.0e-6)
        return {};

    static constexpr std::array<double, 12> major { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static constexpr std::array<double, 12> minor { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    auto correlate = [&chroma] (const std::array<double, 12>& profile, int tonic)
    {
        double meanX = 0.0, meanY = 0.0;

        for (int i = 0; i < 12; ++i)
        {
            meanX += chroma[(size_t) i];
            meanY += profile[(size_t) i];
        }

        meanX /= 12.0;
        meanY /= 12.0;
        double num = 0.0, dx = 0.0, dy = 0.0;

        for (int i = 0; i < 12; ++i)
        {
            auto x = chroma[(size_t) ((i + tonic) % 12)] - meanX;
            auto y = profile[(size_t) i] - meanY;
            num += x * y;
            dx += x * x;
            dy += y * y;
        }

        return num / std::sqrt (dx * dy + 1.0e-12);
    };

    auto bestScore = -2.0;
    juce::String best;

    for (int tonic = 0; tonic < 12; ++tonic)
    {
        if (auto s = correlate (major, tonic); s > bestScore)
        {
            bestScore = s;
            best = names[tonic];
        }

        if (auto s = correlate (minor, tonic); s > bestScore)
        {
            bestScore = s;
            best = juce::String (names[tonic]) + "m";
        }
    }

    return best;
}

void normalise (juce::AudioBuffer<float>& audio, float targetDb)
{
    auto peak = audio.getMagnitude (0, audio.getNumSamples());

    if (peak > 1.0e-5f)
        audio.applyGain (juce::Decibels::decibelsToGain (targetDb) / peak);
}

} // namespace cratedig::analysis
