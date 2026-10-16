#pragma once

#include "Core/Digger.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>

/**
    Instrument plugin: previews the dug record through its output (so it keeps
    running even when the host transport is stopped) and can be retriggered by MIDI notes.
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

    void dig();
    void loadFromHistory (int index);
    bool isBusy() const { return digger.isBusy(); }

    bool hasSample() const { return current.audio != nullptr; }
    const cratedig::LoadedSample& getCurrent() const { return current; }
    const std::vector<cratedig::SampleInfo>& getHistory() const { return history; }

    juce::String getStatus() const { return status; }
    bool isStatusError() const { return statusIsError; }
    void setStatus (const juce::String& text, bool isError = false);

    cratedig::archive::DigFilters getFilters() const;
    void setFilters (const cratedig::archive::DigFilters&);

    juce::Point<int> getEditorSize() const;
    void setEditorSize (juce::Point<int>);

    /** Selection inside the current sample, in samples. Empty = whole sample. */
    juce::Range<int> getSelection() const;
    void setSelection (juce::Range<int>);
    juce::Range<int> getEffectiveSelection() const;

    void play();
    void playFrom (int sample);
    void stop();
    bool isPlaying() const { return playing.load(); }

    /** Playhead position in source samples, or -1 when stopped. */
    double getPlayheadSample() const { return playing.load() ? playhead.load() : -1.0; }

    /** Writes the current selection as a WAV into the export folder (reusing an identical earlier export). */
    juce::File exportSelection (juce::String& error);

    juce::AudioProcessorValueTreeState parameters;

private:
    void handleLoaded (const cratedig::LoadedSample&);
    void addToHistory (const cratedig::SampleInfo&);
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Guards the session data below: some hosts save state from a background thread.
    // It is only written on the message thread, so message-thread reads need no lock.
    mutable juce::CriticalSection stateLock;
    cratedig::archive::DigFilters filters;
    juce::Point<int> editorSize { 980, 640 };

    cratedig::Digger digger;
    cratedig::LoadedSample current;
    std::vector<cratedig::SampleInfo> history;
    juce::String status { "Hit DIG to pull a random record" };
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
