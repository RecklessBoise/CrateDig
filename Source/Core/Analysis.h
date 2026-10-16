#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cratedig::analysis
{

/** Rough tempo estimate from onset-envelope autocorrelation. Returns 0 when nothing rhythmic is found. */
double estimateBpm (const juce::AudioBuffer<float>& audio, double sampleRate);

/** Krumhansl-Schmuckler key estimate, e.g. "C" or "F#m". Returns an empty string for silence. */
juce::String estimateKey (const juce::AudioBuffer<float>& audio, double sampleRate);

/** Scales the buffer so its peak sits at `targetDb`. Silent buffers are left untouched. */
void normalise (juce::AudioBuffer<float>& audio, float targetDb = -1.0f);

} // namespace cratedig::analysis
