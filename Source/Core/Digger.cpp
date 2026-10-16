#include "Digger.h"

#include "Analysis.h"

namespace cratedig
{

namespace
{
    constexpr int maxDigAttempts = 8;
    constexpr int connectionTimeoutMs = 15000;

    juce::String userAgentHeader()
    {
        return "User-Agent: CrateDig/" CRATEDIG_VERSION " (+https://github.com/RecklessBoise/CrateDig)";
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

juce::File Digger::cacheFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
#if JUCE_MAC
        .getChildFile ("Caches")
#endif
        .getChildFile ("CrateDig");
}

void Digger::cancel()
{
    stopThread (connectionTimeoutMs + 2000);
}

void Digger::dig (const archive::DigFilters& newFilters)
{
    if (isThreadRunning())
        return;

    {
        const juce::ScopedLock sl (jobLock);
        job = Job::dig;
        filters = newFilters;
    }

    startThread();
}

void Digger::load (const SampleInfo& info)
{
    if (isThreadRunning() || ! info.isValid())
        return;

    {
        const juce::ScopedLock sl (jobLock);
        job = Job::load;
        requested = info;
    }

    startThread();
}

void Digger::run()
{
    Job currentJob;
    archive::DigFilters currentFilters;
    SampleInfo info;

    {
        const juce::ScopedLock sl (jobLock);
        currentJob = job;
        currentFilters = filters;
        info = requested;
    }

    if (currentJob == Job::dig)
    {
        auto found = findRandomRecord (currentFilters);

        if (! found.has_value())
            return; // error already posted (or cancelled)

        info = *found;
    }
    else if (info.audioFile.isEmpty() && ! resolveAudioFile (info))
    {
        postError ("Could not find audio for this record");
        return;
    }

    auto loaded = fetchAndDecode (info);

    if (! loaded.has_value() || threadShouldExit())
        return;

    juce::MessageManager::callAsync ([weak = weakSelf, result = std::move (*loaded)]
    {
        if (weak != nullptr && weak->onLoaded)
            weak->onLoaded (result);
    });
}

std::optional<SampleInfo> Digger::findRandomRecord (const archive::DigFilters& digFilters)
{
    auto range = archive::yearRange (digFilters);

    if (range.isEmpty())
    {
        postError ("No public-domain records in that decade (only up to " + juce::String (archive::publicDomainLastYear) + ")");
        return std::nullopt;
    }

    bool hadNetworkResponse = false;

    for (int attempt = 0; attempt < maxDigAttempts && ! threadShouldExit(); ++attempt)
    {
        postStatus (attempt == 0 ? "Digging through the crates..." : "Still digging...");

        auto year = range.first + random.nextInt (range.last - range.first + 1);
        auto query = archive::buildQuery (digFilters, year);
        auto& sorts = archive::sortOrders();
        auto sort = sorts[random.nextInt (sorts.size())];

        auto first = archive::parseSearchResponse (httpGet (archive::buildSearchUrl (query, 1, 1, sort)));

        if (! first.has_value())
            continue;

        hadNetworkResponse = true;

        if (first->numFound == 0)
            continue;

        auto offset = random.nextInt (juce::jmin (first->numFound, archive::maxSearchOffset));
        auto page = offset == 0 ? first : archive::parseSearchResponse (httpGet (archive::buildSearchUrl (query, 1, offset + 1, sort)));

        if (! page.has_value() || page->docs.empty())
            continue;

        auto info = page->docs.front();

        if (! resolveAudioFile (info))
            continue;

        if (info.durationSeconds > archive::maxDurationSeconds)
            continue;

        return info;
    }

    if (! threadShouldExit())
        postError (hadNetworkResponse ? "Nothing found with those filters - try widening them"
                                      : "Cannot reach archive.org - check your connection");

    return std::nullopt;
}

bool Digger::resolveAudioFile (SampleInfo& info)
{
    auto choice = archive::pickAudioFile (httpGet (archive::buildFilesUrl (info.identifier)));

    if (! choice.has_value())
        return false;

    info.audioFile = choice->name;
    info.durationSeconds = choice->durationSeconds;
    return true;
}

std::optional<LoadedSample> Digger::fetchAndDecode (const SampleInfo& info)
{
    auto folder = cacheFolder();
    folder.createDirectory();

    auto cached = folder.getChildFile (juce::String::toHexString (info.identifier.hashCode64()) + ".mp3");

    if (! cached.existsAsFile() || cached.getSize() == 0)
    {
        postStatus ("Downloading \"" + info.title + "\"...");

        if (! download (info.downloadUrl(), cached))
        {
            if (! threadShouldExit())
                postError ("Download failed");

            return std::nullopt;
        }
    }

    postStatus ("Decoding...");
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (cached));

    if (reader == nullptr)
    {
        cached.deleteFile();
        postError ("Could not decode the audio file");
        return std::nullopt;
    }

    auto maxSamples = (juce::int64) (reader->sampleRate * archive::maxDurationSeconds);
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

    return result;
}

juce::String Digger::httpGet (const juce::String& url)
{
    int status = 0;
    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs (connectionTimeoutMs)
                       .withExtraHeaders (userAgentHeader())
                       .withNumRedirectsToFollow (5)
                       .withStatusCode (&status);

    if (auto stream = juce::URL (url).createInputStream (options))
        if (status < 400)
            return stream->readEntireStreamAsString();

    return {};
}

bool Digger::download (const juce::String& url, const juce::File& destination)
{
    int status = 0;
    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs (connectionTimeoutMs)
                       .withExtraHeaders (userAgentHeader())
                       .withNumRedirectsToFollow (8)
                       .withStatusCode (&status);

    auto stream = juce::URL (url).createInputStream (options);

    if (stream == nullptr || status >= 400)
        return false;

    auto partial = destination.withFileExtension ("part");
    partial.deleteFile();

    {
        juce::FileOutputStream out (partial);

        if (out.failedToOpen())
            return false;

        constexpr int chunkSize = 1 << 16;
        juce::HeapBlock<char> buffer (chunkSize);
        auto total = stream->getTotalLength();
        juce::int64 received = 0;
        auto lastReport = juce::Time::getMillisecondCounter();

        while (! stream->isExhausted())
        {
            if (threadShouldExit())
            {
                out.flush();
                partial.deleteFile();
                return false;
            }

            auto n = stream->read (buffer, chunkSize);

            if (n <= 0)
                break;

            out.write (buffer, (size_t) n);
            received += n;

            if (total > 0 && juce::Time::getMillisecondCounter() - lastReport > 250)
            {
                lastReport = juce::Time::getMillisecondCounter();
                postStatus ("Downloading... " + juce::String ((int) (100 * received / total)) + "%");
            }
        }

        out.flush();

        if (received == 0 || (total > 0 && received < total))
        {
            partial.deleteFile();
            return false;
        }
    }

    return partial.moveFileTo (destination);
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
