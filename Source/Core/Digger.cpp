#include "Digger.h"

#include "Analysis.h"

#include <algorithm>

namespace cratedig
{

namespace
{
    constexpr int searchTimeoutMs = 45000;
    constexpr int fetchTimeoutMs = 180000;
    constexpr int maxSearchAttempts = 3;
    constexpr int cachedPreviews = 20;

    /** yt-dlp's last line that names an existing file (its --print after_move:filepath output). */
    juce::File lastPrintedFile (const juce::String& output)
    {
        juce::StringArray lines;
        lines.addLines (output);

        for (int i = lines.size(); --i >= 0;)
        {
            auto line = lines[i].trim();

            if (juce::File::isAbsolutePath (line))
                if (juce::File f (line); f.existsAsFile())
                    return f;
        }

        return {};
    }

    /** The most useful line of a yt-dlp failure. */
    juce::String describeFailure (const YtDlp::Result& result)
    {
        if (result.timedOut)
            return "yt-dlp timed out";

        juce::StringArray lines;
        lines.addLines (result.output);

        for (auto& line : lines)
            if (line.startsWith ("ERROR:"))
                return "yt-dlp: " + line.fromFirstOccurrenceOf ("ERROR:", false, false).trim().substring (0, 160);

        return "yt-dlp failed (exit code " + juce::String (result.exitCode) + ")";
    }
} // namespace

Digger::Digger() : juce::Thread ("CrateDig digger")
{
    weakSelf = this; // assigned in the body: masterReference is declared after weakSelf
    formatManager.registerBasicFormats();
}

Digger::~Digger()
{
    cancel();
}

void Digger::setCustomYtDlp (const juce::File& file)
{
    const juce::ScopedLock sl (jobLock);
    customYtDlp = file;
}

juce::File Digger::getCustomYtDlp() const
{
    const juce::ScopedLock sl (jobLock);
    return customYtDlp;
}

bool Digger::isYtDlpAvailable() const
{
    return YtDlp (getCustomYtDlp()).isAvailable();
}

juce::File Digger::cacheFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
#if JUCE_MAC
        .getChildFile ("Caches")
#endif
        .getChildFile ("CrateDig")
        .getChildFile ("Previews");
}

void Digger::cancel()
{
    // The yt-dlp runner polls threadShouldExit, so this returns quickly.
    stopThread (5000);
}

void Digger::search (const youtube::DigFilters& newFilters)
{
    if (isThreadRunning())
        return;

    {
        const juce::ScopedLock sl (jobLock);
        job = Job::search;
        filters = newFilters;
    }

    startThread();
}

void Digger::fetch (const SampleInfo& info)
{
    if (isThreadRunning() || ! youtube::isValidVideoId (info.id))
        return;

    {
        const juce::ScopedLock sl (jobLock);
        job = Job::fetch;
        requested = info;
    }

    startThread();
}

void Digger::run()
{
    Job currentJob;
    youtube::DigFilters currentFilters;
    SampleInfo info;

    {
        const juce::ScopedLock sl (jobLock);
        currentJob = job;
        currentFilters = filters;
        info = requested;
    }

    if (currentJob == Job::search)
        runSearch (currentFilters);
    else
        runFetch (info);
}

void Digger::runSearch (const youtube::DigFilters& searchFilters)
{
    YtDlp ytdlp (getCustomYtDlp());

    if (! ytdlp.isAvailable())
    {
        postError (YtDlp::installHint());
        return;
    }

    auto isRandom = searchFilters.text.trim().isEmpty();
    auto attempts = isRandom ? maxSearchAttempts : 1;

    for (int attempt = 0; attempt < attempts && ! threadShouldExit(); ++attempt)
    {
        auto query = youtube::buildQuery (searchFilters, random);
        postStatus ("Searching YouTube for \"" + query + "\"...");

        auto result = ytdlp.run (youtube::buildSearchArgs (query), searchTimeoutMs, [this] { return threadShouldExit(); });

        if (result.cancelled || threadShouldExit())
            return;

        auto found = youtube::parseSearchOutput (result.output);

        if (found.empty() && ! result.succeeded())
        {
            postError (describeFailure (result));
            return;
        }

        std::vector<SampleInfo> tracks;
        std::copy_if (found.begin(), found.end(), std::back_inserter (tracks), youtube::looksLikeTrack);

        if (tracks.empty())
            continue;

        if (isRandom)
        {
            // Shuffle so the first card is a surprise, like digging through a crate.
            for (auto i = tracks.size(); i > 1; --i)
                std::swap (tracks[i - 1], tracks[(size_t) random.nextInt ((int) i)]);
        }

        juce::MessageManager::callAsync ([weak = weakSelf, query, tracks]
        {
            if (weak != nullptr && weak->onResults)
                weak->onResults (query, tracks);
        });
        return;
    }

    if (! threadShouldExit())
        postError (isRandom ? "Nothing usable found - dig again or change the filters"
                            : "No tracks found for that search");
}

void Digger::runFetch (const SampleInfo& info)
{
    auto file = findCached (info);

    if (! file.existsAsFile())
    {
        YtDlp ytdlp (getCustomYtDlp());

        if (! ytdlp.isAvailable())
        {
            postError (YtDlp::installHint());
            return;
        }

        auto folder = cacheFolder();
        folder.createDirectory();
        pruneCache (cachedPreviews);

        postStatus ("Fetching audio for \"" + info.title + "\"...");

        auto convertToWav = ytdlp.getFfmpeg().existsAsFile();
        auto outputTemplate = folder.getChildFile (info.id + ".%(ext)s").getFullPathName();
        auto result = ytdlp.run (youtube::buildAudioArgs (info.id, outputTemplate, convertToWav),
                                 fetchTimeoutMs, [this] { return threadShouldExit(); });

        if (result.cancelled || threadShouldExit())
            return;

        file = lastPrintedFile (result.output);

        if (! file.existsAsFile())
            file = findCached (info);

        if (! file.existsAsFile())
        {
            postError (describeFailure (result));
            return;
        }
    }

    auto loaded = decode (info, file);

    if (! loaded.has_value() || threadShouldExit())
        return;

    juce::MessageManager::callAsync ([weak = weakSelf, result = std::move (*loaded)]
    {
        if (weak != nullptr && weak->onLoaded)
            weak->onLoaded (result);
    });
}

std::optional<LoadedSample> Digger::decode (const SampleInfo& info, const juce::File& file)
{
    postStatus ("Decoding...");
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
    {
        file.deleteFile();
       #if JUCE_WINDOWS
        postError ("Could not decode this audio - install ffmpeg so yt-dlp can convert it to WAV");
       #else
        postError ("Could not decode this audio");
       #endif
        return std::nullopt;
    }

    auto maxSamples = (juce::int64) (reader->sampleRate * youtube::maxDurationSeconds);
    auto numSamples = (int) juce::jmin (reader->lengthInSamples, maxSamples);
    auto numChannels = (int) juce::jlimit (1u, 2u, reader->numChannels);

    if (numSamples <= 0)
    {
        postError ("The audio file is empty");
        return std::nullopt;
    }

    auto audio = std::make_shared<juce::AudioBuffer<float>> (numChannels, numSamples);
    reader->read (audio.get(), 0, numSamples, 0, true, numChannels > 1);

    if (threadShouldExit())
        return std::nullopt;

    postStatus ("Analysing...");
    analysis::normalise (*audio);

    LoadedSample result;
    result.info = info;
    result.sampleRate = reader->sampleRate;
    result.bpm = analysis::estimateBpm (*audio, reader->sampleRate);
    result.key = analysis::estimateKey (*audio, reader->sampleRate);
    result.audio = std::move (audio);

    if (result.info.durationSeconds <= 0.0)
        result.info.durationSeconds = numSamples / reader->sampleRate;

    file.setLastModificationTime (juce::Time::getCurrentTime()); // keeps recently used previews when pruning
    return result;
}

bool Digger::isCached (const SampleInfo& info)
{
    return findCached (info).existsAsFile();
}

juce::File Digger::findCached (const SampleInfo& info)
{
    if (! youtube::isValidVideoId (info.id))
        return {};

    for (auto& file : cacheFolder().findChildFiles (juce::File::findFiles, false, info.id + ".*"))
        if (file.getFileExtension() != ".part" && file.getSize() > 0)
            return file;

    return {};
}

void Digger::pruneCache (int filesToKeep)
{
    auto files = cacheFolder().findChildFiles (juce::File::findFiles, false);

    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getLastModificationTime() > b.getLastModificationTime();
    });

    for (int i = filesToKeep; i < files.size(); ++i)
        files.getReference (i).deleteFile();
}

void Digger::postStatus (const juce::String& message)
{
    juce::MessageManager::callAsync ([weak = weakSelf, message]
    {
        if (weak != nullptr && weak->onStatus)
            weak->onStatus (message);
    });
}

void Digger::postError (const juce::String& message)
{
    juce::MessageManager::callAsync ([weak = weakSelf, message]
    {
        if (weak != nullptr && weak->onError)
            weak->onError (message);
    });
}

} // namespace cratedig
