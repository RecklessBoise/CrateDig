#pragma once

#include "SampleInfo.h"

#include <optional>
#include <vector>

namespace cratedig::archive
{

/** The Internet Archive collection we dig in: the Great 78 Project (George Blood transfers). */
inline constexpr const char* collection = "georgeblood";

inline constexpr int earliestYear = 1898;
inline constexpr int latestYear = 1960;

/** US public domain cut-off for sound recordings (Music Modernization Act, as of 2026). */
inline constexpr int publicDomainLastYear = 1925;

/** archive.org refuses to page past this many results. */
inline constexpr int maxSearchOffset = 9999;

/** Longest recording we accept, in seconds. */
inline constexpr double maxDurationSeconds = 600.0;

struct DigFilters
{
    juce::String genre;            // empty = any
    int decade = 0;                // e.g. 1930, 0 = any
    bool publicDomainOnly = false;
};

struct YearRange
{
    int first = earliestYear;
    int last = latestYear;

    bool isEmpty() const noexcept { return last < first; }
};

struct SearchResult
{
    int numFound = 0;
    std::vector<SampleInfo> docs;
};

struct AudioFileChoice
{
    juce::String name;
    double durationSeconds = 0.0;
};

/** Genres offered in the UI. These match archive.org "subject" values in the collection. */
const juce::StringArray& genres();

/** Decades offered in the UI (1900 ... 1950). */
juce::Array<int> decades();

YearRange yearRange (const DigFilters& filters);

/** Builds the Lucene query for one specific year. */
juce::String buildQuery (const DigFilters& filters, int year);

/** Builds an advancedsearch URL returning `rows` documents starting at 1-based `page`. */
juce::String buildSearchUrl (const juce::String& query, int rows, int page, const juce::String& sort);

juce::String buildFilesUrl (const juce::String& identifier);

/** Sort orders we pick from at random so that capped paging still reaches the whole collection. */
const juce::StringArray& sortOrders();

std::optional<SearchResult> parseSearchResponse (const juce::String& json);

/** Picks the MP3 rendition of the recording from a /metadata/{id}/files response. */
std::optional<AudioFileChoice> pickAudioFile (const juce::String& filesJson);

} // namespace cratedig::archive
