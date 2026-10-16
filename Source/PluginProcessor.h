#pragma once

#include "Core/Digger.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>

/**
    Instrument plugin: previews the chosen YouTube track through its output (so it keeps
    running even when the host transport is stopped) and can be retriggered by MIDI notes.

    Flow: search() lists results -> select*() picks a card -> listen() fetches a temporary preview
    -> download() / drag writes a WAV into the user's library. Nothing is fetched or saved implicitly.
*/
class CrateDigProcessor final : public juce::AudioProcessor,
                                public juce::ChangeBroadcaster
{
public:
    CrateDigProcessor();
    ~CrateDigProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Message-thread API used by the editor.

    void search();
    void selectResult (int index);
    void selectHistory (int index);

    /** Plays the selected track, fetching its preview first if needed. */
    void listen();

    /** Saves the selection (or whole track) as WAV, fetching the preview first if needed. */
    void download();

    bool isBusy() const { return digger.isBusy(); }
    bool isYtDlpAvailable() const { return digger.isYtDlpAvailable(); }
    void setCustomYtDlp (const juce::File&);
    juce::File getCustomYtDlp() const { return digger.getCustomYtDlp(); }

    const cratedig::SampleInfo& getSelected() const { return selected; }
    const std::vector<cratedig::SampleInfo>& getResults() const { return results; }
    const std::vector<cratedig::SampleInfo>& getHistory() const { return history; }
    juce::String getLastQuery() const { return lastQuery; }

    /** The decoded preview, which may belong to a different card than the selected one. */
    const cratedig::LoadedSample& getLoaded() const { return loaded; }
    bool isSelectedLoaded() const;

    juce::String getStatus() const { return status; }
    bool isStatusError() const { return statusIsError; }
    void setStatus (const juce::String& text, bool isError = false);

    cratedig::youtube::DigFilters getFilters() const;
    void setFilters (const cratedig::youtube::DigFilters&);

    juce::Point<int> getEditorSize() const;
    void setEditorSize (juce::Point<int>);

    /** Selection inside the loaded preview, in samples. Empty = whole sample. */
    juce::Range<int> getSelection() const;
    void setSelection (juce::Range<int>);
    juce::Range<int> getEffectiveSelection() const;

    void play();
    void playFrom (int sample);
    void stop();
    bool isPlaying() const { return playing.load(); }

    /** Playhead position in source samples, or -1 when stopped. */
    double getPlayheadSample() const { return playing.load() ? playhead.load() : -1.0; }

    /** Writes the current selection of the loaded preview as a WAV into the library folder. */
    juce::File exportSelection (juce::String& error);

    juce::AudioProcessorValueTreeState parameters;

private:
    void handleResults (const juce::String& query, const std::vector<cratedig::SampleInfo>&);
    void handleLoaded (const cratedig::LoadedSample&);
    void select (const cratedig::SampleInfo&);
    void addToHistory (const cratedig::SampleInfo&);
    void saveGlobalSettings();
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Guards the session data below: some hosts save state from a background thread.
    // It is only written on the message thread, so message-thread reads need no lock.
    mutable juce::CriticalSection stateLock;
    cratedig::youtube::DigFilters filters;
    juce::Point<int> editorSize { 980, 640 };
    std::vector<cratedig::SampleInfo> results, history;
    cratedig::SampleInfo selected;
    juce::String lastQuery;

    cratedig::Digger digger;
    cratedig::LoadedSample loaded;
    bool downloadWhenLoaded = false;
    juce::String status;
    bool statusIsError = false;

    // Audio-thread shared state. The preview buffer pointer is swapped under a spin lock that the
    // audio thread only ever try-locks; the old buffer is released on the message thread.
    juce::SpinLock previewLock;
    std::shared_ptr<const juce::AudioBuffer<float>> preview;
    double previewSampleRate = 44100.0;

    std::atomic<int> selectionStart { 0 }, selectionEnd { 0 };
    std::atomic<bool> playing { false };
    std::atomic<int> startRequest { -1 }; // -2 = from selection start, >= 0 = absolute sample
    std::atomic<bool> stopRequest { false };
    std::atomic<double> playhead { 0.0 };

    double hostSampleRate = 44100.0;
    double readPosition = 0.0;
    juce::SmoothedValue<float> gainSmoothed;

    std::atomic<float>* gainParam = nullptr;
    std::atomic<float>* loopParam = nullptr;

    static constexpr int maxHistory = 30;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrateDigProcessor)
};
