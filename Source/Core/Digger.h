#pragma once

#include "YouTube.h"
#include "YtDlp.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <functional>
#include <memory>
#include <vector>

namespace cratedig
{

/** A decoded, analysed preview ready for playback and export. */
struct LoadedSample
{
    SampleInfo info;
    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    double sampleRate = 0.0;
    double bpm = 0.0;
    juce::String key;
};

/**
    Runs yt-dlp on a background thread. Two separate jobs, both started by the user:

    - search(): asks YouTube for results. No audio is fetched.
    - fetch():  pulls the audio of ONE chosen video into a temporary preview cache, decodes and analyses it.
                Nothing reaches the user's library until they press Download or drag the sample out.

    Callbacks are always delivered on the message thread.
*/
class Digger : private juce::Thread
{
public:
    Digger();
    ~Digger() override;

    /** Path to a yt-dlp chosen by the user (empty = search the usual install folders). */
    void setCustomYtDlp (const juce::File& file);
    juce::File getCustomYtDlp() const;
    bool isYtDlpAvailable() const;

    /** Searches YouTube. Ignored while busy. */
    void search (const youtube::DigFilters& filters);

    /** Fetches the audio of one video for preview. Ignored while busy. */
    void fetch (const SampleInfo& info);

    /** True when the preview of this video is already in the cache (no network needed). */
    static bool isCached (const SampleInfo& info);

    bool isBusy() const { return isThreadRunning(); }
    void cancel();

    std::function<void (const juce::String& query, const std::vector<SampleInfo>& results)> onResults;
    std::function<void (const LoadedSample&)> onLoaded;
    std::function<void (const juce::String&)> onError;
    std::function<void (const juce::String&)> onStatus;

    /** Temporary preview cache. Old files are pruned automatically. */
    static juce::File cacheFolder();

private:
    void run() override;

    void runSearch (const youtube::DigFilters&);
    void runFetch (const SampleInfo&);
    std::optional<LoadedSample> decode (const SampleInfo&, const juce::File&);

    static juce::File findCached (const SampleInfo&);
    static void pruneCache (int filesToKeep);

    void postStatus (const juce::String&);
    void postError (const juce::String&);

    enum class Job { search, fetch };

    juce::CriticalSection jobLock;
    Job job = Job::search;
    youtube::DigFilters filters;
    SampleInfo requested;
    juce::File customYtDlp;

    juce::AudioFormatManager formatManager;
    juce::Random random;

    // Created on the message thread so the background thread only ever copies it.
    juce::WeakReference<Digger> weakSelf;

    JUCE_DECLARE_WEAK_REFERENCEABLE (Digger)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Digger)
};

} // namespace cratedig
