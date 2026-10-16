#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace cratedig::ui
{

/**
    Shows the record's waveform, the playhead and the chop selection.
    Drag to select a chop, drag a selection edge to adjust it, click to play from a point,
    double-click to clear the selection.
*/
class WaveformView final : public juce::Component,
                           private juce::Timer
{
public:
    WaveformView();

    void setAudio (std::shared_ptr<const juce::AudioBuffer<float>> audio, double sampleRate);
    void setSelection (juce::Range<int> selection);
    void setScale (float newScale) { scale = newScale; repaint(); }
    void setPlaceholder (const juce::String& text) { placeholder = text; repaint(); }

    std::function<void (juce::Range<int>)> onSelectionChanged;
    std::function<void (int sample)> onClick;
    std::function<double()> getPlayhead;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    juce::Rectangle<float> waveArea() const;
    int xToSample (float x) const;
    float sampleToX (double sample) const;
    juce::String formatTime (double seconds) const;

    enum class DragMode { none, create, startEdge, endEdge };

    std::vector<std::pair<float, float>> peaks; // min/max per bin
    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    double sampleRate = 44100.0;
    int totalSamples = 0;

    juce::Range<int> selection;
    DragMode dragMode = DragMode::none;
    int dragAnchor = 0;
    bool dragged = false;

    double lastPlayhead = -1.0;
    float scale = 1.0f;
    juce::String placeholder { "No record yet - hit DIG" };

    static constexpr int numBins = 4096;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};

} // namespace cratedig::ui
