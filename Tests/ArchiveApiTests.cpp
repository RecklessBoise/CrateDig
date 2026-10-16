#include "Core/ArchiveApi.h"

using namespace cratedig;

class ArchiveApiTests final : public juce::UnitTest
{
public:
    ArchiveApiTests() : juce::UnitTest ("Archive API", "CrateDig") {}

    void runTest() override
    {
        beginTest ("Year range follows decade and public-domain filters");
        {
            archive::DigFilters f;
            expectEquals (archive::yearRange (f).first, archive::earliestYear);
            expectEquals (archive::yearRange (f).last, archive::latestYear);

            f.decade = 1930;
            expectEquals (archive::yearRange (f).first, 1930);
            expectEquals (archive::yearRange (f).last, 1939);

            f.publicDomainOnly = true;
            expect (archive::yearRange (f).isEmpty());

            f.decade = 1920;
            expectEquals (archive::yearRange (f).last, archive::publicDomainLastYear);
        }

        beginTest ("Query includes collection, genre and year");
        {
            archive::DigFilters f;
            f.genre = "Jazz";
            auto q = archive::buildQuery (f, 1931);
            expect (q.contains ("collection:georgeblood"));
            expect (q.contains ("subject:\"Jazz\""));
            expect (q.contains ("year:1931"));

            f.genre = "Bad\"Genre";
            expect (! archive::buildQuery (f, 0).contains ("Bad\"Genre"));
            expect (! archive::buildQuery (f, 0).contains ("year:"));
        }

        beginTest ("Search URL is percent-encoded");
        {
            auto url = archive::buildSearchUrl ("collection:georgeblood AND year:1931", 1, 42, "downloads desc");
            expect (url.startsWith ("https://archive.org/advancedsearch.php?q=collection%3Ageorgeblood%20AND%20year%3A1931"));
            expect (url.contains ("&sort%5B%5D=downloads%20desc"));
            expect (url.contains ("&rows=1&page=42&output=json"));
            expect (! url.contains (" "));
        }

        beginTest ("Download URL escapes file names");
        {
            SampleInfo info;
            info.identifier = "78_test_gbia0001";
            info.audioFile = "MISSISSIPPI MILDRED - JELLY-ROLL MORTON'S.mp3";
            expectEquals (info.downloadUrl(),
                          juce::String ("https://archive.org/download/78_test_gbia0001/MISSISSIPPI%20MILDRED%20-%20JELLY-ROLL%20MORTON%27S.mp3"));
        }

        beginTest ("Parses search responses with string or array fields");
        {
            auto json = R"({"response":{"numFound":7490,"start":0,"docs":[
                {"creator":["JELLY-ROLL MORTON'S RED HOT PEPPERS","Henry Allen"],"identifier":"78_a","subject":["78rpm","Jazz"],"title":"MISSISSIPPI MILDRED","year":1930},
                {"creator":"Solo Artist","identifier":"78_b","subject":"78rpm;Blues","title":"Song","year":"1928"},
                {"title":"no identifier"}]}})";

            auto result = archive::parseSearchResponse (json);
            expect (result.has_value());
            expectEquals (result->numFound, 7490);
            expectEquals ((int) result->docs.size(), 2);
            expectEquals (result->docs[0].creator, juce::String ("JELLY-ROLL MORTON'S RED HOT PEPPERS, Henry Allen"));
            expectEquals (result->docs[0].genre(), juce::String ("Jazz"));
            expectEquals (result->docs[1].year, 1928);
            expectEquals (result->docs[1].genre(), juce::String ("Blues"));
        }

        beginTest ("Rejects malformed search responses");
        {
            expect (! archive::parseSearchResponse ("").has_value());
            expect (! archive::parseSearchResponse ("{\"error\":\"[DEEP_PAGING]\"}").has_value());
        }

        beginTest ("Picks the VBR MP3 from the file list");
        {
            auto json = R"({"result":[
                {"name":"x.flac","format":"24bit Flac","length":"185.64"},
                {"name":"x_64kb.mp3","format":"64Kbps MP3","length":"185.7"},
                {"name":"x.mp3","format":"VBR MP3","length":"185.69"},
                {"name":"x.png","format":"PNG"}]})";

            auto choice = archive::pickAudioFile (json);
            expect (choice.has_value());
            expectEquals (choice->name, juce::String ("x.mp3"));
            expectWithinAbsoluteError (choice->durationSeconds, 185.69, 0.001);

            expect (! archive::pickAudioFile (R"({"result":[{"name":"x.flac","format":"Flac"}]})").has_value());
            expect (! archive::pickAudioFile ("garbage").has_value());
        }

        beginTest ("SampleInfo round-trips through a ValueTree");
        {
            SampleInfo info;
            info.identifier = "id";
            info.title = "Title";
            info.creator = "Artist";
            info.audioFile = "file.mp3";
            info.subjects = { "78rpm", "Jazz" };
            info.year = 1931;
            info.durationSeconds = 183.5;

            auto copy = SampleInfo::fromValueTree (info.toValueTree());
            expectEquals (copy.identifier, info.identifier);
            expectEquals (copy.audioFile, info.audioFile);
            expect (copy.subjects == info.subjects);
            expectEquals (copy.year, 1931);
            expectWithinAbsoluteError (copy.durationSeconds, 183.5, 1.0e-9);
        }
    }
};

static ArchiveApiTests archiveApiTests;
