#include "YouTube.h"

namespace cratedig::youtube
{

namespace
{
    const char* const searchPrintTemplate = "%(id)s\t%(duration)s\t%(view_count)s\t%(channel,uploader)s\t%(title)s";
} // namespace

const juce::StringArray& genres()
{
    static const juce::StringArray list {
        "Soul", "Funk", "Jazz", "Jazz Funk", "Disco", "Boogie", "Gospel", "Blues", "Bossa Nova",
        "Samba", "MPB", "Latin", "Salsa", "Afrobeat", "Highlife", "Ethiopian Jazz", "Reggae", "Dub",
        "Library Music", "Soundtrack", "Psych Rock", "Turkish Psych", "Prog Rock", "Folk",
        "City Pop", "Italo Disco", "French Pop", "Synth", "Ambient", "Lounge", "Exotica"
    };
    return list;
}

juce::Array<int> decades()
{
    juce::Array<int> list;

    for (int d = 1950; d <= 2000; d += 10)
        list.add (d);

    return list;
}

juce::String buildRandomQuery (const DigFilters& filters, juce::Random& random)
{
    static const juce::StringArray flavours { "rare", "vinyl", "45", "LP", "obscure", "private press",
                                              "b-side", "original", "rare groove", "vinyl rip" };

    auto& allGenres = genres();
    auto genre = filters.genre.isNotEmpty() ? filters.genre : allGenres[random.nextInt (allGenres.size())];

    // Without a decade, favour the golden era of sampled records.
    auto decade = filters.decade > 0 ? filters.decade : 1960 + 10 * random.nextInt (3);
    auto year = decade + random.nextInt (10);

    juce::StringArray words;

    if (random.nextBool())
        words.add (juce::String (year));
    else
        words.add (juce::String (decade) + "s");

    words.add (genre.toLowerCase());
    words.add (flavours[random.nextInt (flavours.size())]);
    return words.joinIntoString (" ");
}

juce::String buildQuery (const DigFilters& filters, juce::Random& random)
{
    auto text = filters.text.trim();

    if (text.isEmpty())
        return buildRandomQuery (filters, random);

    juce::StringArray words { text };

    if (filters.genre.isNotEmpty())
        words.add (filters.genre.toLowerCase());

    if (filters.decade > 0)
        words.add (juce::String (filters.decade) + "s");

    return words.joinIntoString (" ");
}

juce::StringArray buildSearchArgs (const juce::String& query, int count)
{
    // The query is a single argv entry: no shell is involved, so it cannot inject options.
    auto sanitised = query.replaceCharacters ("\r\n\t", "   ").trim();

    // One compact tab-separated line per result keeps the output far below the pipe buffer size.
    return { "--ignore-config", "--no-warnings", "--flat-playlist", "--print", searchPrintTemplate,
             "ytsearch" + juce::String (juce::jmax (1, count)) + ":" + sanitised };
}

std::vector<SampleInfo> parseSearchOutput (const juce::String& output)
{
    std::vector<SampleInfo> results;
    juce::StringArray lines;
    lines.addLines (output);

    auto number = [] (const juce::String& field) { return field == "NA" ? juce::String() : field.trim(); };

    for (auto& line : lines)
    {
        auto fields = juce::StringArray::fromTokens (line, "\t", {});

        if (fields.size() < 5 || ! isValidVideoId (fields[0].trim()))
            continue;

        SampleInfo info;
        info.id = fields[0].trim();
        info.durationSeconds = number (fields[1]).getDoubleValue();
        info.viewCount = number (fields[2]).isEmpty() ? -1 : number (fields[2]).getLargeIntValue();
        info.channel = fields[3] == "NA" ? juce::String() : fields[3].trim();

        // The title is last, so any stray tab inside it is kept.
        fields.removeRange (0, 4);
        info.title = fields.joinIntoString (" ").trim();
        info.year = guessYear (info.title);
        results.push_back (std::move (info));
    }

    return results;
}

bool looksLikeTrack (const SampleInfo& info)
{
    if (info.durationSeconds < minDurationSeconds || info.durationSeconds > maxDurationSeconds)
        return false;

    static const juce::StringArray blocked { "top 10", "top 20", "top 50", "top 100", "review", "unboxing",
                                             "reaction", "my collection", "record haul", "tutorial", "how to",
                                             "podcast", "interview", "#shorts", "documentary", "lesson",
                                             "turntable", "record player", "price guide", "most valuable" };

    auto title = info.title.toLowerCase();

    for (auto& word : blocked)
        if (title.contains (word))
            return false;

    return true;
}

int guessYear (const juce::String& title)
{
    for (int i = 0; i + 4 <= title.length(); ++i)
    {
        auto candidate = title.substring (i, i + 4);

        if (! candidate.containsOnly ("0123456789"))
            continue;

        juce::juce_wchar before = i > 0 ? title[i - 1] : ' ';
        juce::juce_wchar after = i + 4 < title.length() ? title[i + 4] : ' ';

        if (juce::CharacterFunctions::isDigit (before) || juce::CharacterFunctions::isDigit (after))
            continue;

        auto year = candidate.getIntValue();

        if (year >= 1900 && year <= 2029)
            return year;
    }

    return 0;
}

bool isValidVideoId (const juce::String& id)
{
    return id.length() == 11
        && id.containsOnly ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-");
}

juce::StringArray buildAudioArgs (const juce::String& videoId, const juce::String& outputTemplate, bool convertToWav)
{
    jassert (isValidVideoId (videoId));

    juce::StringArray args { "--ignore-config", "--no-warnings", "--no-playlist", "--no-part", "--no-progress",
                             "--max-filesize", "200M" };

    if (convertToWav)
        args.addArray ({ "-f", "bestaudio/best", "-x", "--audio-format", "wav" });
    else
        args.addArray ({ "-f", "bestaudio[ext=m4a]/bestaudio[acodec^=mp4a]/bestaudio" });

    args.addArray ({ "-o", outputTemplate, "--print", "after_move:filepath",
                     "https://www.youtube.com/watch?v=" + videoId });
    return args;
}

} // namespace cratedig::youtube
