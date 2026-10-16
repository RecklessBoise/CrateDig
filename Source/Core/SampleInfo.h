#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace cratedig
{

/** Metadata for one record pulled from the Internet Archive. */
struct SampleInfo
{
    juce::String identifier;
    juce::String title;
    juce::String creator;
    juce::String audioFile;      // file name inside the archive.org item
    juce::StringArray subjects;  // genres / tags
    int year = 0;
    double durationSeconds = 0.0;

    bool isValid() const noexcept { return identifier.isNotEmpty(); }

    /** The first subject that is more specific than the "78rpm" collection tag. */
    juce::String genre() const;

    juce::String pageUrl() const;
    juce::String downloadUrl() const;

    juce::ValueTree toValueTree() const;
    static SampleInfo fromValueTree (const juce::ValueTree& tree);
};

/** Percent-encodes everything except RFC 3986 unreserved characters. */
juce::String percentEncode (const juce::String& text);

} // namespace cratedig
