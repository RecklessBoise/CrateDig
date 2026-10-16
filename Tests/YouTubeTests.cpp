#include "Core/YouTube.h"
#include "Core/YtDlp.h"

using namespace cratedig;

class YouTubeTests final : public juce::UnitTest
{
public:
    YouTubeTests() : juce::UnitTest ("YouTube", "CrateDig") {}

    void runTest() override
    {
        beginTest ("Random queries respect genre and decade");
        {
            juce::Random random (7);
            youtube::DigFilters filters;
            filters.genre = "Bossa Nova";
            filters.decade = 1970;

            for (int i = 0; i < 50; ++i)
            {
                auto query = youtube::buildRandomQuery (filters, random);
                expect (query.contains ("bossa nova"), query);
                auto year = query.upToFirstOccurrenceOf (" ", false, false).getIntValue();
                expect (year >= 1970 && year <= 1979, query);
            }

            youtube::DigFilters any;
            expect (youtube::buildRandomQuery (any, random).isNotEmpty());
        }

        beginTest ("Free text wins over random digging");
        {
            juce::Random random (1);
            youtube::DigFilters filters;
            filters.text = "  turkish psych  ";
            filters.decade = 1970;
            expectEquals (youtube::buildQuery (filters, random), juce::String ("turkish psych 1970s"));
        }

        beginTest ("Search arguments keep the query in a single argument");
        {
            auto args = youtube::buildSearchArgs ("soul\n--exec rm", 10);
            expect (args.contains ("--flat-playlist"));
            expectEquals (args[args.size() - 1], juce::String ("ytsearch10:soul --exec rm"));
            expect (! args.contains ("--exec"));
        }

        beginTest ("Parses search output and skips noise");
        {
            juce::String output;
            output << "WARNING: [youtube] something\n"
                   << "uiDR1Q6c024\t211\t1063\tLoop Diggin' / GiL\tDaniel Janin - Love Life | Jazz Funk 1974\n"
                   << "KlKuaIW7WPc\tNA\tNA\tNA\tUntitled\twith tab\n"
                   << "bad-id\t200\t1\tx\ty\n"
                   << "ERROR: oops\n";

            auto results = youtube::parseSearchOutput (output);
            expectEquals ((int) results.size(), 2);
            expectEquals (results[0].id, juce::String ("uiDR1Q6c024"));
            expectEquals (results[0].durationSeconds, 211.0);
            expectEquals (results[0].viewCount, (juce::int64) 1063);
            expectEquals (results[0].channel, juce::String ("Loop Diggin' / GiL"));
            expectEquals (results[0].year, 1974);
            expectEquals (results[1].durationSeconds, 0.0);
            expectEquals (results[1].viewCount, (juce::int64) -1);
            expect (results[1].channel.isEmpty());
            expectEquals (results[1].title, juce::String ("Untitled with tab"));
        }

        beginTest ("Track filter rejects shorts, long videos and non-music");
        {
            SampleInfo info;
            info.title = "Rare Soul 45 - Mighty Good Man";
            info.durationSeconds = 193.0;
            expect (youtube::looksLikeTrack (info));

            info.durationSeconds = 20.0;
            expect (! youtube::looksLikeTrack (info));

            info.durationSeconds = 2400.0;
            expect (! youtube::looksLikeTrack (info));

            info.durationSeconds = 200.0;
            info.title = "Top 100 Rarest Vinyl Records";
            expect (! youtube::looksLikeTrack (info));
        }

        beginTest ("Year guessing");
        {
            expectEquals (youtube::guessYear ("Soviet Jazz-Funk, 1974"), 1974);
            expectEquals (youtube::guessYear ("Track 12345 remix"), 0);
            expectEquals (youtube::guessYear ("Live 1888"), 0);
            expectEquals (youtube::guessYear ("No year"), 0);
        }

        beginTest ("Video id validation");
        {
            expect (youtube::isValidVideoId ("uiDR1Q6c024"));
            expect (youtube::isValidVideoId ("a-b_c-d_e-f"));
            expect (! youtube::isValidVideoId ("uiDR1Q6c02"));
            expect (! youtube::isValidVideoId ("--exec=rm -"));
            expect (! youtube::isValidVideoId ("uiDR1Q6c02;"));
        }

        beginTest ("Audio arguments");
        {
            auto m4a = youtube::buildAudioArgs ("uiDR1Q6c024", "/tmp/x/%(id)s.%(ext)s", false);
            expectEquals (m4a[m4a.size() - 1], juce::String ("https://www.youtube.com/watch?v=uiDR1Q6c024"));
            expect (m4a.contains ("after_move:filepath"));
            expect (! m4a.contains ("-x"));

            auto wav = youtube::buildAudioArgs ("uiDR1Q6c024", "/tmp/x/%(id)s.%(ext)s", true);
            expect (wav.contains ("-x"));
            expect (wav.contains ("wav"));
        }

        beginTest ("URLs");
        {
            SampleInfo info;
            info.id = "uiDR1Q6c024";
            expectEquals (info.watchUrl(), juce::String ("https://www.youtube.com/watch?v=uiDR1Q6c024"));
            expectEquals (info.thumbnailUrl(), juce::String ("https://i.ytimg.com/vi/uiDR1Q6c024/mqdefault.jpg"));
        }

        beginTest ("SampleInfo round-trips through a ValueTree");
        {
            SampleInfo info;
            info.id = "uiDR1Q6c024";
            info.title = "Love Life";
            info.channel = "Daniel Janin";
            info.durationSeconds = 211.0;
            info.viewCount = 5000000000LL;
            info.year = 1974;

            auto copy = SampleInfo::fromValueTree (info.toValueTree());
            expectEquals (copy.id, info.id);
            expectEquals (copy.title, info.title);
            expectEquals (copy.channel, info.channel);
            expectEquals (copy.viewCount, info.viewCount);
            expectEquals (copy.year, 1974);
        }

        beginTest ("yt-dlp locator honours a custom path");
        {
            auto dir = juce::File::createTempFile ("cratedig-ytdlp");
            dir.createDirectory();
           #if JUCE_WINDOWS
            auto fake = dir.getChildFile ("my-yt-dlp.exe"); // findTool appends .exe on Windows
           #else
            auto fake = dir.getChildFile ("my-yt-dlp");
           #endif
            fake.replaceWithText ("#!/bin/sh\necho hello\n");

            YtDlp custom (fake);
            expect (custom.isAvailable());
            expect (custom.getExecutable() == fake);

            expect (YtDlp::findTool ("my-yt-dlp", { dir }) == fake);
            expect (! YtDlp::findTool ("missing-tool", { dir }).exists());

            dir.deleteRecursively();
        }
    }
};

static YouTubeTests youTubeTests;
