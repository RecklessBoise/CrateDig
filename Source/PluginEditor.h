#pragma once

#include "PluginProcessor.h"
#include "UI/CrateLookAndFeel.h"
#include "UI/DragTile.h"
#include "UI/WaveformView.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <map>

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

    // ListBoxModel (search results or history)
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    void timerCallback() override;

    void refreshFromProcessor();
    void updateFiltersFromUi();
    void applySizePreset (float factor);
    void locateYtDlp();
    void requestThumbnail (const cratedig::SampleInfo&);
    const std::vector<cratedig::SampleInfo>& visibleList() const;

    CrateDigProcessor& crate;
    cratedig::ui::CrateLookAndFeel lookAndFeel;
    float scale = 1.0f;

    juce::TextButton sizeSmall { "S" }, sizeMedium { "M" }, sizeLarge { "L" };
    juce::TextButton ytdlpButton { "yt-dlp..." };
    juce::TextButton openFolderButton { "Open folder" };

    juce::TextEditor searchBox;
    juce::ComboBox genreBox, decadeBox;
    juce::TextButton digButton { "DIG" };

    juce::HyperlinkButton youtubeLink { "Open on YouTube", {} };
    cratedig::ui::WaveformView waveform;

    juce::TextButton listenButton { "Listen" };
    juce::ToggleButton loopToggle { "Loop" };
    juce::Slider gainSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };

    cratedig::ui::DragTile dragTile;
    juce::TextButton downloadButton { "Download" };

    juce::TextButton resultsTab { "Results" }, historyTab { "History" };
    juce::ListBox list { "List", this };
    bool showingHistory = false;

    juce::Label statusLabel;
    juce::TooltipWindow tooltips { this, 600 };

    juce::AudioProcessorValueTreeState::ButtonAttachment loopAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;

    std::unique_ptr<juce::FileChooser> chooser;

    // Thumbnails are fetched on a worker thread and cached per video id.
    juce::ThreadPool thumbnailPool { 1 };
    std::map<juce::String, juce::Image> thumbnails;
    juce::StringArray requestedThumbnails;

    // Layout rectangles painted directly in paint().
    juce::Rectangle<int> headerArea, infoArea, thumbArea, listHeaderArea, gainLabelArea;
    int busyTicks = 0;
    const juce::AudioBuffer<float>* shownAudio = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrateDigEditor)
};
