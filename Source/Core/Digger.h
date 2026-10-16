#pragma once

#include "ArchiveApi.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <functional>
#include <memory>

namespace cratedig
{

/** A decoded, analysed record ready for preview and export. */
struct LoadedSample
{
    SampleInfo info;
    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    double sampleRate = 0.0;
    double bpm = 0.0;
    juce::String key;
};

/**
    Runs all network and decoding work on a background thread:
    random search on archive.org -> pick the MP3 -> download into the cache -> decode -> analyse.

    Callbacks are always delivered on the message thread.
*/
class Digger : private juce::Thread
{
public:
    Digger();
    ~Digger() override;

    /** Finds a random record matching the filters. Ignored while busy. */
    void dig (const archive::DigFilters& filters);

    /** Loads a known record (from the cache when possible). Ignored while busy. */
    void load (const SampleInfo& info);

    bool isBusy() const { return isThreadRunning(); }
    void cancel();

    std::function<void (const LoadedSample&)> onLoaded;
    std::function<void (const juce::String&)> onError;
    std::function<void (const juce::String&)> onStatus;

    static juce::File cacheFolder();

private:
    void run() override;

    std::optional<SampleInfo> findRandomRecord (const archive::DigFilters&);
    bool resolveAudioFile (SampleInfo&);
    std::optional<LoadedSample> fetchAndDecode (const SampleInfo&);

    juce::String httpGet (const juce::String& url);
    bool download (const juce::String& url, const juce::File& destination);

    void postStatus (const juce::String&);
    void postError (const juce::String&);

    enum class Job { dig, load };

    juce::CriticalSection jobLock;
    Job job = Job::dig;
    archive::DigFilters filters;
    SampleInfo requested;

    juce::AudioFormatManager formatManager;
    juce::Random random;

    // Created on the message thread so the background thread only ever copies it.
    juce::WeakReference<Digger> weakSelf;

    JUCE_DECLARE_WEAK_REFERENCEABLE (Digger)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Digger)
};

} // namespace cratedig
