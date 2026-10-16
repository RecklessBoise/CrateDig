#pragma once

#include "SampleInfo.h"

#include <vector>

namespace cratedig::youtube
{

/** Shortest / longest video we treat as a usable track, in seconds. */
inline constexpr double minDurationSeconds = 45.0;
inline constexpr double maxDurationSeconds = 900.0;

/** How many results a single search asks yt-dlp for. */
inline constexpr int searchResultCount = 30;

struct DigFilters
{
    juce::String text;   // free-text search; empty = random dig
    juce::String genre;  // empty = any
    int decade = 0;      // e.g. 1970, 0 = any
};

/** Genres offered in the UI, crate-digging flavoured. */
const juce::StringArray& genres();

/** Decades offered in the UI (1950 ... 2000). */
juce::Array<int> decades();

/** A random samplette-style query such as "1974 jazz funk rare vinyl" built from the filters. */
juce::String buildRandomQuery (const DigFilters& filters, juce::Random& random);

/** The query to run for these filters: the free text (plus filters) when present, otherwise a random one. */
juce::String buildQuery (const DigFilters& filters, juce::Random& random);

/** yt-dlp arguments (without the executable) for a flat search printing "id\tduration\tviews\tchannel\ttitle" per line. */
juce::StringArray buildSearchArgs (const juce::String& query, int count = searchResultCount);

/** Parses the search output. Malformed lines (warnings, errors) are ignored. */
std::vector<SampleInfo> parseSearchOutput (const juce::String& output);

/** Rejects shorts, mixes, reviews, unboxings and other non-track results. */
bool looksLikeTrack (const SampleInfo& info);

/** A 4-digit year (1900-2029) found in a title, or 0. */
int guessYear (const juce::String& title);

/** True for an 11-character id made of [A-Za-z0-9_-]. Only such ids are ever passed to yt-dlp. */
bool isValidVideoId (const juce::String& id);

/**
    yt-dlp arguments that fetch the audio of one video into `outputTemplate` and print the final path.
    With ffmpeg available the audio is converted to WAV, otherwise the m4a stream is kept as-is.
*/
juce::StringArray buildAudioArgs (const juce::String& videoId, const juce::String& outputTemplate, bool convertToWav);

} // namespace cratedig::youtube
