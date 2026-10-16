#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace cratedig
{

/** Metadata for one YouTube video found by a search. No audio is attached to it. */
struct SampleInfo
{
    juce::String id;        // 11-character YouTube video id
    juce::String title;
    juce::String channel;
    double durationSeconds = 0.0;
    juce::int64 viewCount = -1;
    int year = 0;           // guessed from the title, 0 when unknown

    bool isValid() const noexcept { return id.isNotEmpty(); }

    juce::String watchUrl() const;
    juce::String thumbnailUrl() const;

    juce::ValueTree toValueTree() const;
    static SampleInfo fromValueTree (const juce::ValueTree& tree);
};

} // namespace cratedig
