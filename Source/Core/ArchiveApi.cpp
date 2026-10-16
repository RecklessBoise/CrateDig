#include "ArchiveApi.h"

namespace cratedig::archive
{

namespace
{
    juce::String joinVar (const juce::var& value, const juce::String& separator)
    {
        if (auto* array = value.getArray())
        {
            juce::StringArray parts;

            for (auto& item : *array)
                parts.add (item.toString().trim());

            parts.removeEmptyStrings();
            return parts.joinIntoString (separator);
        }

        return value.isVoid() ? juce::String() : value.toString().trim();
    }

    juce::StringArray toStringArray (const juce::var& value)
    {
        juce::StringArray result;

        if (auto* array = value.getArray())
        {
            for (auto& item : *array)
                result.add (item.toString().trim());
        }
        else if (! value.isVoid())
        {
            result.addTokens (value.toString(), ";", {});
            result.trim();
        }

        result.removeEmptyStrings();
        return result;
    }

    juce::String escapeQueryValue (const juce::String& text)
    {
        return text.removeCharacters ("\"\\");
    }
} // namespace

const juce::StringArray& genres()
{
    static const juce::StringArray list {
        "Jazz", "Blues", "Popular Music", "Country", "Latin", "Instrumental", "Orchestral",
        "Dance", "Fox Trot", "Waltz", "Tango", "Polka", "Opera", "Vocal",
        "Religious", "Ethnic", "Library Music", "Film"
    };
    return list;
}

juce::Array<int> decades()
{
    juce::Array<int> list;

    for (int d = 1900; d <= 1950; d += 10)
        list.add (d);

    return list;
}

YearRange yearRange (const DigFilters& filters)
{
    YearRange range;

    if (filters.decade > 0)
    {
        range.first = juce::jmax (earliestYear, filters.decade);
        range.last = juce::jmin (latestYear, filters.decade + 9);
    }

    if (filters.publicDomainOnly)
        range.last = juce::jmin (range.last, publicDomainLastYear);

    return range;
}

juce::String buildQuery (const DigFilters& filters, int year)
{
    juce::String query = "collection:" + juce::String (collection) + " AND mediatype:audio";

    if (filters.genre.isNotEmpty())
        query << " AND subject:\"" << escapeQueryValue (filters.genre) << "\"";

    if (year > 0)
        query << " AND year:" << year;

    return query;
}

juce::String buildSearchUrl (const juce::String& query, int rows, int page, const juce::String& sort)
{
    juce::String url = "https://archive.org/advancedsearch.php?q=" + percentEncode (query);

    for (auto* field : { "identifier", "title", "creator", "year", "subject" })
        url << "&fl%5B%5D=" << field;

    if (sort.isNotEmpty())
        url << "&sort%5B%5D=" << percentEncode (sort);

    url << "&rows=" << rows << "&page=" << juce::jmax (1, page) << "&output=json";
    return url;
}

juce::String buildFilesUrl (const juce::String& identifier)
{
    return "https://archive.org/metadata/" + percentEncode (identifier) + "/files";
}

const juce::StringArray& sortOrders()
{
    static const juce::StringArray list { "", "downloads desc", "identifier asc", "identifier desc",
                                          "titleSorter asc", "addeddate desc" };
    return list;
}

std::optional<SearchResult> parseSearchResponse (const juce::String& json)
{
    auto root = juce::JSON::parse (json);
    auto response = root.getProperty ("response", {});

    if (! response.isObject())
        return std::nullopt;

    SearchResult result;
    result.numFound = (int) response.getProperty ("numFound", 0);

    if (auto* docs = response.getProperty ("docs", {}).getArray())
    {
        for (auto& doc : *docs)
        {
            SampleInfo info;
            info.identifier = doc.getProperty ("identifier", {}).toString();

            if (info.identifier.isEmpty())
                continue;

            info.title = joinVar (doc.getProperty ("title", {}), " / ");
            info.creator = joinVar (doc.getProperty ("creator", {}), ", ");
            info.subjects = toStringArray (doc.getProperty ("subject", {}));
            info.year = doc.getProperty ("year", 0).toString().getIntValue();

            if (info.title.isEmpty())
                info.title = info.identifier;

            result.docs.push_back (std::move (info));
        }
    }

    return result;
}

std::optional<AudioFileChoice> pickAudioFile (const juce::String& filesJson)
{
    auto root = juce::JSON::parse (filesJson);
    auto* files = root.getProperty ("result", {}).getArray();

    if (files == nullptr)
        return std::nullopt;

    std::optional<AudioFileChoice> fallback;

    for (auto& file : *files)
    {
        auto name = file.getProperty ("name", {}).toString();
        auto format = file.getProperty ("format", {}).toString();

        if (! name.endsWithIgnoreCase (".mp3"))
            continue;

        AudioFileChoice choice { name, file.getProperty ("length", {}).toString().getDoubleValue() };

        // Prefer the main VBR MP3; derivatives such as 64Kbps samples are a fallback.
        if (format.equalsIgnoreCase ("VBR MP3"))
            return choice;

        if (! fallback.has_value())
            fallback = choice;
    }

    return fallback;
}

} // namespace cratedig::archive
