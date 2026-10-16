#pragma once

#include "PluginProcessor.h"
#include "UI/CrateLookAndFeel.h"
#include "UI/DragTile.h"
#include "UI/WaveformView.h"

#include <juce_audio_processors/juce_audio_processors.h>

class CrateDigEditor final : public juce::AudioProcessorEditor,
                             private juce::ChangeListener,
                             private juce::ListBoxModel,
                             private juce::Timer
{
public:
    static constexpr int defaultWidth = 980, defaultHeight = 640;
    static constexpr int minWidth = 760, minHeight = 500;
    static constexpr int maxWidth = 2200, maxHeight = 1400;

    explicit CrateDigEditor (CrateDigProcessor&);
    ~CrateDigEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // ChangeListener
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    // ListBoxModel (dig history)
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    void timerCallback() override;

    void refreshFromProcessor();
    void updateFiltersFromUi();
    void showMessage (const juce::String& text, bool isError);
    void saveWav();
    void applySizePreset (float factor);

    CrateDigProcessor& crate;
    cratedig::ui::CrateLookAndFeel lookAndFeel;
    float scale = 1.0f;

    juce::TextButton sizeSmall { "S" }, sizeMedium { "M" }, sizeLarge { "L" };
    juce::TextButton openFolderButton { "Open folder" };

    juce::ComboBox genreBox, decadeBox;
    juce::ToggleButton publicDomainToggle { "Public domain only" };
    juce::TextButton digButton { "DIG" };

    juce::HyperlinkButton archiveLink { "archive.org", {} };
    cratedig::ui::WaveformView waveform;

    juce::TextButton playButton { "Play" };
    juce::ToggleButton loopToggle { "Loop" };
    juce::Slider gainSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };

    cratedig::ui::DragTile dragTile;
    juce::TextButton saveButton { "Save WAV" };

    juce::ListBox historyList { "History", this };

    juce::Label statusLabel;
    juce::TooltipWindow tooltips { this, 600 };

    juce::AudioProcessorValueTreeState::ButtonAttachment loopAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;

    // Layout rectangles painted directly in paint().
    juce::Rectangle<int> headerArea, infoArea, historyHeaderArea, gainLabelArea;
    int busyTicks = 0;
    const juce::AudioBuffer<float>* shownAudio = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrateDigEditor)
};
