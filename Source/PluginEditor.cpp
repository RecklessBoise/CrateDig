#include "PluginEditor.h"

#include "Core/Exporter.h"

using namespace cratedig;
namespace colours = cratedig::ui::colours;

namespace
{
    constexpr int anyId = 1;

    juce::String formatDuration (double seconds)
    {
        auto total = juce::roundToInt (seconds);
        return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
    }

    juce::String formatViews (juce::int64 views)
    {
        if (views < 0)
            return {};

        if (views >= 1000000)
            return juce::String ((double) views / 1.0e6, 1) + "M views";

        if (views >= 1000)
            return juce::String ((double) views / 1.0e3, 1) + "K views";

        return juce::String (views) + " views";
    }

    void drawChip (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& label,
                   const juce::String& value, float scale)
    {
        g.setColour (colours::panelRaised);
        g.fillRoundedRectangle (area, 6.0f * scale);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (area.reduced (0.5f), 6.0f * scale, 1.0f);

        auto top = area.reduced (8.0f * scale, 4.0f * scale);
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (10.5f * scale, juce::Font::bold));
        g.drawText (label, top.removeFromTop (top.getHeight() * 0.38f), juce::Justification::centred);
        g.setColour (value == "-" ? colours::textDim : colours::accent);
        g.setFont (juce::FontOptions (19.0f * scale, juce::Font::bold));
        g.drawText (value, top, juce::Justification::centred);
    }
} // namespace

CrateDigEditor::CrateDigEditor (CrateDigProcessor& p)
    : AudioProcessorEditor (p),
      crate (p),
      loopAttachment (p.parameters, "loop", loopToggle),
      gainAttachment (p.parameters, "gain", gainSlider)
{
    setLookAndFeel (&lookAndFeel);

    // Header
    for (auto* b : { &sizeSmall, &sizeMedium, &sizeLarge })
    {
        addAndMakeVisible (b);
        b->setTooltip ("Window size preset - you can also drag the bottom-right corner");
    }

    sizeSmall.onClick = [this] { applySizePreset (0.78f); };
    sizeMedium.onClick = [this] { applySizePreset (1.0f); };
    sizeLarge.onClick = [this] { applySizePreset (1.3f); };

    addAndMakeVisible (ytdlpButton);
    ytdlpButton.onClick = [this] { locateYtDlp(); };
    ytdlpButton.setTooltip ("Choose which yt-dlp executable CrateDig uses");

    addAndMakeVisible (openFolderButton);
    openFolderButton.setTooltip ("Your downloads: " + exporter::defaultFolder().getFullPathName());
    openFolderButton.onClick = []
    {
        auto folder = exporter::defaultFolder();
        folder.createDirectory();
        folder.startAsProcess();
    };

    // Search + filters
    addAndMakeVisible (searchBox);
    searchBox.setTextToShowWhenEmpty ("Search YouTube - or leave empty and DIG at random", colours::textDim);
    searchBox.setColour (juce::TextEditor::backgroundColourId, colours::panelRaised);
    searchBox.setColour (juce::TextEditor::textColourId, colours::text);
    searchBox.setColour (juce::TextEditor::outlineColourId, colours::outline);
    searchBox.setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
    searchBox.setColour (juce::CaretComponent::caretColourId, colours::accent);
    searchBox.setSelectAllWhenFocused (true);
    searchBox.onTextChange = [this] { updateFiltersFromUi(); };
    searchBox.onReturnKey = [this] { crate.search(); };

    addAndMakeVisible (genreBox);
    genreBox.addItem ("Any genre", anyId);
    genreBox.addItemList (youtube::genres(), anyId + 1);

    addAndMakeVisible (decadeBox);
    decadeBox.addItem ("Any decade", anyId);

    for (auto decade : youtube::decades())
        decadeBox.addItem (juce::String (decade) + "s", decade);

    genreBox.onChange = [this] { updateFiltersFromUi(); };
    decadeBox.onChange = [this] { updateFiltersFromUi(); };

    addAndMakeVisible (digButton);
    digButton.setComponentID ("dig");
    digButton.setColour (juce::TextButton::buttonColourId, colours::accent);
    digButton.setColour (juce::TextButton::textColourOffId, colours::background);
    digButton.setTooltip ("Search YouTube. Nothing is downloaded until you press Listen or Download.");
    digButton.onClick = [this] { crate.search(); };

    addAndMakeVisible (youtubeLink);
    youtubeLink.setTooltip ("Watch it on YouTube in your browser");

    // Waveform
    addAndMakeVisible (waveform);
    waveform.getPlayhead = [this] { return crate.isSelectedLoaded() ? crate.getPlayheadSample() : -1.0; };
    waveform.onSelectionChanged = [this] (juce::Range<int> sel)
    {
        crate.setSelection (sel);
        refreshFromProcessor();
    };
    waveform.onClick = [this] (int sample) { crate.playFrom (sample); };

    // Transport
    addAndMakeVisible (listenButton);
    listenButton.setTooltip ("Fetch a temporary preview and play it");
    listenButton.onClick = [this]
    {
        if (crate.isSelectedLoaded() && crate.isPlaying())
            crate.stop();
        else
            crate.listen();
    };

    addAndMakeVisible (loopToggle);
    addAndMakeVisible (gainSlider);
    gainSlider.setTooltip ("Preview level (does not affect downloaded files)");
    gainSlider.setPopupDisplayEnabled (true, true, this);

    // Export
    addAndMakeVisible (dragTile);
    dragTile.prepareFile = [this] (juce::String& error) { return crate.exportSelection (error); };
    dragTile.onMessage = [this] (const juce::String& text, bool isError) { crate.setStatus (text, isError); };

    addAndMakeVisible (downloadButton);
    downloadButton.setTooltip ("Save the chop (or the whole track) as WAV into " + exporter::defaultFolder().getFullPathName());
    downloadButton.onClick = [this] { crate.download(); };

    // Results / history
    for (auto* tab : { &resultsTab, &historyTab })
    {
        addAndMakeVisible (tab);
        tab->setColour (juce::TextButton::buttonColourId, colours::panel);
    }

    resultsTab.onClick = [this] { showingHistory = false; refreshFromProcessor(); };
    historyTab.onClick = [this] { showingHistory = true; refreshFromProcessor(); };

    addAndMakeVisible (list);
    list.setOutlineThickness (0);
    list.setColour (juce::ListBox::backgroundColourId, colours::panel);
    list.setTooltip ("Click to select, double-click to listen");

    addAndMakeVisible (statusLabel);
    statusLabel.setJustificationType (juce::Justification::centredLeft);

    crate.addChangeListener (this);

    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, maxWidth, maxHeight);
    auto savedSize = crate.getEditorSize();
    setSize (savedSize.x, savedSize.y);

    refreshFromProcessor();
    startTimerHz (10);
}

CrateDigEditor::~CrateDigEditor()
{
    thumbnailPool.removeAllJobs (true, 6000);
    crate.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

//==============================================================================
void CrateDigEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // Header: logo + tagline
    auto header = headerArea.toFloat();
    g.setColour (colours::accent);
    g.setFont (juce::FontOptions (24.0f * scale, juce::Font::bold));
    g.drawText ("CRATEDIG", header.removeFromLeft (130.0f * scale), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (12.5f * scale));
    g.drawText ("dig YouTube for samples - you decide what gets downloaded", header, juce::Justification::centredLeft, true);

    // Info card
    auto card = infoArea.toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (card, 8.0f * scale);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (card.reduced (0.5f), 8.0f * scale, 1.0f);

    auto& selected = crate.getSelected();
    auto thumb = thumbArea.toFloat();

    if (selected.isValid())
    {
        auto it = thumbnails.find (selected.id);

        if (it != thumbnails.end() && it->second.isValid())
        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addRoundedRectangle (thumb, 6.0f * scale);
            g.reduceClipRegion (clip);
            g.drawImage (it->second, thumb, juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
        }
        else
        {
            g.setColour (colours::panelRaised);
            g.fillRoundedRectangle (thumb, 6.0f * scale);
        }
    }

    auto inner = card.reduced (14.0f * scale, 10.0f * scale);

    if (selected.isValid())
        inner.setLeft (thumb.getRight() + 14.0f * scale);

    auto chipWidth = 74.0f * scale;
    auto chips = inner.removeFromRight (chipWidth * 2.0f + 8.0f * scale);
    inner.removeFromRight (12.0f * scale);

    if (selected.isValid())
    {
        g.setColour (colours::text);
        g.setFont (juce::FontOptions (17.0f * scale, juce::Font::bold));
        g.drawFittedText (selected.title, inner.removeFromTop (inner.getHeight() * 0.5f).toNearestInt(),
                          juce::Justification::bottomLeft, 2, 0.9f);

        g.setColour (colours::textDim.brighter (0.3f));
        g.setFont (juce::FontOptions (13.5f * scale));
        g.drawText (selected.channel, inner.removeFromTop (inner.getHeight() * 0.45f), juce::Justification::centredLeft, true);

        juce::StringArray meta;

        if (selected.durationSeconds > 0.0)
            meta.add (formatDuration (selected.durationSeconds));

        if (auto views = formatViews (selected.viewCount); views.isNotEmpty())
            meta.add (views);

        if (selected.year > 0)
            meta.add (juce::String (selected.year));

        g.setFont (juce::FontOptions (12.5f * scale));
        g.setColour (colours::textDim);
        g.drawText (meta.joinIntoString ("  -  "), inner, juce::Justification::topLeft, true);
    }
    else
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (15.0f * scale));
        g.drawText ("Type a search or pick a genre / decade, then hit DIG.", inner, juce::Justification::centredLeft, true);
    }

    auto& loaded = crate.getLoaded();
    auto isLoaded = crate.isSelectedLoaded();
    auto chipArea = chips.removeFromTop (chips.getHeight() - 22.0f * scale);
    auto bpm = isLoaded && loaded.bpm > 0.0 ? juce::String (juce::roundToInt (loaded.bpm)) : juce::String ("-");
    auto key = isLoaded && loaded.key.isNotEmpty() ? loaded.key : juce::String ("-");
    drawChip (g, chipArea.removeFromLeft (chipWidth), "BPM", bpm, scale);
    chipArea.removeFromLeft (8.0f * scale);
    drawChip (g, chipArea.removeFromLeft (chipWidth), "KEY", key, scale);

    // Gain label
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (12.5f * scale));
    g.drawText ("Gain", gainLabelArea, juce::Justification::centredRight);

    // Results / history panel
    g.setColour (colours::panel);
    g.fillRoundedRectangle (listHeaderArea.getUnion (list.getBounds()).toFloat().expanded (0.0f, 2.0f), 8.0f * scale);

    if (! showingHistory && crate.getLastQuery().isNotEmpty())
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (11.0f * scale));
        auto queryArea = listHeaderArea.withTrimmedTop (listHeaderArea.getHeight() * 6 / 10).reduced (juce::roundToInt (12.0f * scale), 0);
        g.drawText ("\"" + crate.getLastQuery() + "\"", queryArea, juce::Justification::centredLeft, true);
    }
}

void CrateDigEditor::resized()
{
    scale = juce::jmin ((float) getWidth() / (float) defaultWidth, (float) getHeight() / (float) defaultHeight);
    scale = juce::jlimit (0.75f, 2.2f, scale);
    lookAndFeel.setScale (scale);
    waveform.setScale (scale);
    dragTile.setScale (scale);

    crate.setEditorSize ({ getWidth(), getHeight() });

    auto s = [this] (float v) { return juce::roundToInt (v * scale); };
    auto bounds = getLocalBounds().reduced (s (14.0f));

    // Header
    headerArea = bounds.removeFromTop (s (36.0f));
    auto headerRight = headerArea;
    openFolderButton.setBounds (headerRight.removeFromRight (s (110.0f)).reduced (0, s (4.0f)));
    headerRight.removeFromRight (s (8.0f));
    ytdlpButton.setBounds (headerRight.removeFromRight (s (100.0f)).reduced (0, s (4.0f)));
    headerRight.removeFromRight (s (14.0f));

    for (auto* b : { &sizeLarge, &sizeMedium, &sizeSmall })
    {
        b->setBounds (headerRight.removeFromRight (s (30.0f)).reduced (0, s (4.0f)));
        headerRight.removeFromRight (s (4.0f));
    }

    headerArea.setRight (headerRight.getRight());
    bounds.removeFromTop (s (8.0f));

    // Search row
    auto filterRow = bounds.removeFromTop (s (34.0f));
    decadeBox.setBounds (filterRow.removeFromRight (s (140.0f)));
    filterRow.removeFromRight (s (10.0f));
    genreBox.setBounds (filterRow.removeFromRight (s (170.0f)));
    filterRow.removeFromRight (s (10.0f));
    searchBox.setBounds (filterRow);
    searchBox.applyFontToAllText (juce::FontOptions (14.0f * scale));
    searchBox.setIndents (s (10.0f), juce::jmax (0, (filterRow.getHeight() - s (16.0f)) / 2));
    bounds.removeFromTop (s (12.0f));

    // Status line
    auto statusArea = bounds.removeFromBottom (s (22.0f));
    bounds.removeFromBottom (s (6.0f));
    statusLabel.setBounds (statusArea);
    statusLabel.setFont (juce::FontOptions (13.0f * scale));

    // Results / history column
    auto side = bounds.removeFromRight (juce::jmax (s (220.0f), bounds.getWidth() * 28 / 100));
    bounds.removeFromRight (s (12.0f));
    listHeaderArea = side.removeFromTop (s (52.0f));
    auto tabs = listHeaderArea.withHeight (s (32.0f)).reduced (s (6.0f), s (5.0f));
    resultsTab.setBounds (tabs.removeFromLeft (tabs.getWidth() / 2).withTrimmedRight (s (3.0f)));
    historyTab.setBounds (tabs.withTrimmedLeft (s (3.0f)));
    list.setBounds (side.reduced (s (4.0f), 0).withTrimmedBottom (s (6.0f)));
    list.setRowHeight (s (50.0f));

    // Info row: DIG + card
    auto infoRow = bounds.removeFromTop (s (100.0f));
    digButton.setBounds (infoRow.removeFromLeft (s (120.0f)));
    infoRow.removeFromLeft (s (12.0f));
    infoArea = infoRow;
    auto thumbHeight = infoArea.getHeight() - s (20.0f);
    thumbArea = juce::Rectangle<int> (infoArea.getX() + s (10.0f), infoArea.getY() + s (10.0f), thumbHeight * 16 / 9, thumbHeight);
    youtubeLink.setFont (juce::FontOptions (12.0f * scale), false, juce::Justification::centredRight);
    youtubeLink.setBounds (infoArea.reduced (s (14.0f), s (10.0f)).removeFromBottom (s (18.0f)).removeFromRight (s (160.0f)));
    bounds.removeFromTop (s (12.0f));

    // Export row
    auto exportRow = bounds.removeFromBottom (s (62.0f));
    downloadButton.setBounds (exportRow.removeFromRight (s (120.0f)));
    exportRow.removeFromRight (s (12.0f));
    dragTile.setBounds (exportRow);
    bounds.removeFromBottom (s (10.0f));

    // Transport row
    auto transport = bounds.removeFromBottom (s (32.0f));
    listenButton.setBounds (transport.removeFromLeft (s (100.0f)));
    transport.removeFromLeft (s (14.0f));
    loopToggle.setBounds (transport.removeFromLeft (s (80.0f)));
    gainSlider.setBounds (transport.removeFromRight (s (170.0f)));
    gainLabelArea = transport.removeFromRight (s (50.0f)).withTrimmedRight (s (8.0f));
    bounds.removeFromBottom (s (10.0f));

    waveform.setBounds (bounds);

    list.updateContent();
    repaint();
}

//==============================================================================
void CrateDigEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshFromProcessor();
}

void CrateDigEditor::refreshFromProcessor()
{
    auto filters = crate.getFilters();

    if (! searchBox.hasKeyboardFocus (false) && searchBox.getText() != filters.text)
        searchBox.setText (filters.text, juce::dontSendNotification);

    auto genreIndex = youtube::genres().indexOf (filters.genre);
    genreBox.setSelectedId (genreIndex >= 0 ? genreIndex + anyId + 1 : anyId, juce::dontSendNotification);
    decadeBox.setSelectedId (filters.decade > 0 ? filters.decade : anyId, juce::dontSendNotification);

    if (! crate.isBusy())
        digButton.setButtonText (filters.text.trim().isEmpty() ? "DIG" : "SEARCH");

    auto& selected = crate.getSelected();
    auto& loaded = crate.getLoaded();
    auto isLoaded = crate.isSelectedLoaded();

    auto* audio = isLoaded ? loaded.audio.get() : nullptr;

    if (audio != shownAudio)
    {
        shownAudio = audio;
        waveform.setAudio (isLoaded ? loaded.audio : nullptr, loaded.sampleRate);
    }

    waveform.setSelection (isLoaded ? crate.getSelection() : juce::Range<int>());

    if (crate.isBusy())
        waveform.setPlaceholder ("Working...");
    else if (selected.isValid())
        waveform.setPlaceholder ("Press Listen to preview - nothing is downloaded until you do");
    else
        waveform.setPlaceholder ("No track yet - hit DIG");

    youtubeLink.setVisible (selected.isValid());
    youtubeLink.setURL (juce::URL (selected.watchUrl()));

    if (selected.isValid())
        requestThumbnail (selected);

    dragTile.setEnabled (isLoaded);
    downloadButton.setEnabled (selected.isValid());

    if (isLoaded)
    {
        auto sel = crate.getEffectiveSelection();
        auto suffix = exporter::regionSuffix (sel, loaded.audio->getNumSamples(), loaded.sampleRate);
        dragTile.setSubtitle (exporter::makeBaseName (loaded.info, loaded.bpm, loaded.key) + suffix + ".wav");
    }
    else
    {
        dragTile.setSubtitle (selected.isValid() ? "Listen first, then drag the chop out" : "Dig a track first");
    }

    resultsTab.setToggleState (! showingHistory, juce::dontSendNotification);
    historyTab.setToggleState (showingHistory, juce::dontSendNotification);
    resultsTab.setButtonText ("Results (" + juce::String ((int) crate.getResults().size()) + ")");

    statusLabel.setText (crate.getStatus(), juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, crate.isStatusError() ? colours::error : colours::textDim);

    list.updateContent();
    list.repaint();
    repaint();
}

void CrateDigEditor::updateFiltersFromUi()
{
    auto filters = crate.getFilters();
    filters.text = searchBox.getText();
    auto genreId = genreBox.getSelectedId();
    filters.genre = genreId > anyId ? youtube::genres()[genreId - anyId - 1] : juce::String();
    filters.decade = decadeBox.getSelectedId() > anyId ? decadeBox.getSelectedId() : 0;
    crate.setFilters (filters);

    if (! crate.isBusy())
        digButton.setButtonText (filters.text.trim().isEmpty() ? "DIG" : "SEARCH");
}

void CrateDigEditor::applySizePreset (float factor)
{
    setSize (juce::jlimit (minWidth, maxWidth, juce::roundToInt (defaultWidth * factor)),
             juce::jlimit (minHeight, maxHeight, juce::roundToInt (defaultHeight * factor)));
}

void CrateDigEditor::locateYtDlp()
{
    auto start = crate.getCustomYtDlp().existsAsFile() ? crate.getCustomYtDlp()
                                                       : YtDlp().getExecutable();

    chooser = std::make_unique<juce::FileChooser> ("Locate the yt-dlp executable",
                                                   start.existsAsFile() ? start : juce::File ("/opt/homebrew/bin"));

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis = juce::Component::SafePointer<CrateDigEditor> (this)] (const juce::FileChooser& fc)
                          {
                              if (safeThis == nullptr)
                                  return;

                              auto file = fc.getResult();

                              if (file.existsAsFile())
                                  safeThis->crate.setCustomYtDlp (file);
                          });
}

void CrateDigEditor::requestThumbnail (const SampleInfo& info)
{
    if (requestedThumbnails.contains (info.id))
        return;

    requestedThumbnails.add (info.id);

    thumbnailPool.addJob ([safeThis = juce::Component::SafePointer<CrateDigEditor> (this), id = info.id, url = info.thumbnailUrl()]
    {
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs (5000);
        juce::Image image;

        if (auto stream = juce::URL (url).createInputStream (options))
        {
            juce::MemoryBlock data;
            stream->readIntoMemoryBlock (data, 2 * 1024 * 1024);
            image = juce::ImageFileFormat::loadFrom (data.getData(), data.getSize());
        }

        juce::MessageManager::callAsync ([safeThis, id, image]
        {
            if (safeThis != nullptr)
            {
                safeThis->thumbnails[id] = image;
                safeThis->repaint();
            }
        });
    });
}

void CrateDigEditor::timerCallback()
{
    auto busy = crate.isBusy();
    digButton.setEnabled (! busy);
    listenButton.setEnabled (crate.getSelected().isValid() && (! busy || crate.isSelectedLoaded()));

    if (busy)
    {
        busyTicks = (busyTicks + 1) % 4;
        digButton.setButtonText (juce::String::repeatedString (".", busyTicks + 1));
    }
    else if (digButton.getButtonText().startsWithChar ('.'))
    {
        refreshFromProcessor();
    }

    if (crate.isSelectedLoaded())
        listenButton.setButtonText (crate.isPlaying() ? "Stop" : "Play");
    else
        listenButton.setButtonText ("Listen");
}

//==============================================================================
const std::vector<SampleInfo>& CrateDigEditor::visibleList() const
{
    return showingHistory ? crate.getHistory() : crate.getResults();
}

int CrateDigEditor::getNumRows()
{
    return (int) visibleList().size();
}

void CrateDigEditor::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool)
{
    auto& items = visibleList();

    if (! juce::isPositiveAndBelow (row, (int) items.size()))
        return;

    auto& item = items[(size_t) row];
    auto isSelected = item.id == crate.getSelected().id;
    auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (2.0f * scale, 2.0f * scale);

    if (isSelected)
    {
        g.setColour (colours::accent.withAlpha (0.15f));
        g.fillRoundedRectangle (area, 6.0f * scale);
    }

    auto text = area.reduced (10.0f * scale, 5.0f * scale);

    if (Digger::isCached (item))
    {
        // Small dot: the preview is already on disk, listening is instant.
        auto dot = text.removeFromRight (10.0f * scale);
        g.setColour (colours::accent.withAlpha (0.8f));
        g.fillEllipse (juce::Rectangle<float> (6.0f * scale, 6.0f * scale).withCentre (dot.getCentre()));
    }

    g.setColour (isSelected ? colours::accent : colours::text);
    g.setFont (juce::FontOptions (13.0f * scale, juce::Font::bold));
    g.drawText (item.title, text.removeFromTop (text.getHeight() * 0.55f), juce::Justification::bottomLeft, true);

    juce::String sub = item.channel;

    if (item.durationSeconds > 0.0)
        sub = formatDuration (item.durationSeconds) + "  -  " + sub;

    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.5f * scale));
    g.drawText (sub, text, juce::Justification::topLeft, true);
}

void CrateDigEditor::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (showingHistory)
        crate.selectHistory (row);
    else
        crate.selectResult (row);
}

void CrateDigEditor::listBoxItemDoubleClicked (int row, const juce::MouseEvent& e)
{
    listBoxItemClicked (row, e);
    crate.listen();
}
