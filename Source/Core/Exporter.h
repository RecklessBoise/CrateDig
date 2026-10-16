#pragma once

#include "SampleInfo.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace cratedig::exporter
{

/** Removes characters that are illegal or awkward in file names and collapses whitespace. */
juce::String sanitiseFileName (const juce::String& name, int maxLength = 120);

/** "Artist - Title (1931) [120 BPM, Am]" */
juce::String makeBaseName (const SampleInfo& info, double bpm, const juce::String& key);

/** Suffix describing a region, e.g. " (chop 12.3s-18.0s)". Empty when the region is the whole buffer. */
juce::String regionSuffix (juce::Range<int> region, int totalSamples, double sampleRate);

/** ~/Music/CrateDig */
juce::File defaultFolder();

/**
    Writes `region` of `audio` as a 24-bit WAV into `folder`.
    If an identical export (same name and non-empty) already exists it is reused.
    Returns the written file, or an empty File with `error` set.
*/
juce::File exportWav (const juce::AudioBuffer<float>& audio,
                      double sampleRate,
                      juce::Range<int> region,
                      const juce::File& folder,
                      const juce::String& baseName,
                      juce::String& error);

} // namespace cratedig::exporter
