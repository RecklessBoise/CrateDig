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

    addAndMakeVisible (openFolderButton);
    openFolderButton.setTooltip (exporter::defaultFolder().getFullPathName());
    openFolderButton.onClick = []
    {
        auto folder = exporter::defaultFolder();
        folder.createDirectory();
        folder.startAsProcess();
    };

    // Filters
    addAndMakeVisible (genreBox);
    genreBox.addItem ("Any genre", anyId);
    genreBox.addItemList (archive::genres(), anyId + 1);
    genreBox.setTooltip ("Genre (archive.org subject)");

    addAndMakeVisible (decadeBox);
    decadeBox.addItem ("Any decade", anyId);

    for (auto decade : archive::decades())
        decadeBox.addItem (juce::String (decade) + "s", decade);

    addAndMakeVisible (publicDomainToggle);
    publicDomainToggle.setTooltip ("Only records released up to " + juce::String (archive::publicDomainLastYear)
                                   + " - public domain in the US. Always check the rights before releasing music.");

    genreBox.onChange = [this] { updateFiltersFromUi(); };
    decadeBox.onChange = [this] { updateFiltersFromUi(); };
    publicDomainToggle.onClick = [this] { updateFiltersFromUi(); };

    addAndMakeVisible (digButton);
    digButton.setComponentID ("dig");
    digButton.setColour (juce::TextButton::buttonColourId, colours::accent);
    digButton.setColour (juce::TextButton::textColourOffId, colours::background);
    digButton.setTooltip ("Pull a random record from the crates");
    digButton.onClick = [this] { crate.dig(); };

    addAndMakeVisible (archiveLink);
    archiveLink.setTooltip ("Open the record's page on the Internet Archive");

    // Waveform
    addAndMakeVisible (waveform);
    waveform.getPlayhead = [this] { return crate.getPlayheadSample(); };
    waveform.onSelectionChanged = [this] (juce::Range<int> sel)
    {
        crate.setSelection (sel);
        refreshFromProcessor();
    };
    waveform.onClick = [this] (int sample) { crate.playFrom (sample); };

    // Transport
    addAndMakeVisible (playButton);
    playButton.onClick = [this]
    {
        if (crate.isPlaying())
            crate.stop();
        else
            crate.play();
    };

    addAndMakeVisible (loopToggle);
    addAndMakeVisible (gainSlider);
    gainSlider.setTooltip ("Preview level (does not affect exported files)");
    gainSlider.setPopupDisplayEnabled (true, true, this);

    // Export
    addAndMakeVisible (dragTile);
    dragTile.prepareFile = [this] (juce::String& error) { return crate.exportSelection (error); };
    dragTile.onMessage = [this] (const juce::String& text, bool isError) { showMessage (text, isError); };

    addAndMakeVisible (saveButton);
    saveButton.setTooltip ("Save the selection (or the whole record) as WAV into ~/Music/CrateDig");
    saveButton.onClick = [this] { saveWav(); };

    // History
    addAndMakeVisible (historyList);
    historyList.setOutlineThickness (0);
    historyList.setColour (juce::ListBox::backgroundColourId, colours::panel);

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
    auto logoWidth = 130.0f * scale;
    g.drawText ("CRATEDIG", header.removeFromLeft (logoWidth), juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (12.5f * scale));
    g.drawText ("random records from the Internet Archive's Great 78 Project", header, juce::Justification::centredLeft, true);

    // Info card
    auto card = infoArea.toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (card, 8.0f * scale);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (card.reduced (0.5f), 8.0f * scale, 1.0f);

    auto inner = card.reduced (14.0f * scale, 10.0f * scale);
    auto chipWidth = 74.0f * scale;
    auto chips = inner.removeFromRight (chipWidth * 2.0f + 8.0f * scale);
    inner.removeFromRight (12.0f * scale);

    auto& current = crate.getCurrent();

    if (crate.hasSample())
    {
        auto& info = current.info;
        g.setColour (colours::text);
        g.setFont (juce::FontOptions (20.0f * scale, juce::Font::bold));
        g.drawText (info.title, inner.removeFromTop (inner.getHeight() * 0.42f), juce::Justification::bottomLeft, true);

        g.setColour (colours::textDim.brighter (0.3f));
        g.setFont (juce::FontOptions (14.0f * scale));
        g.drawText (info.creator.isNotEmpty() ? info.creator : juce::String ("Unknown artist"),
                    inner.removeFromTop (inner.getHeight() * 0.5f), juce::Justification::centredLeft, true);

        juce::StringArray meta;

        if (info.year > 0)
            meta.add (juce::String (info.year));

        if (info.genre().isNotEmpty())
            meta.add (info.genre());

        meta.add (formatDuration (current.audio->getNumSamples() / current.sampleRate));
        g.setFont (juce::FontOptions (12.5f * scale));
        g.setColour (colours::textDim);
        g.drawText (meta.joinIntoString ("  -  "), inner, juce::Justification::topLeft, true);
    }
    else
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (15.0f * scale));
        g.drawText ("Pick a genre or decade (or don't), then hit DIG.", inner, juce::Justification::centredLeft, true);
    }

    auto chipArea = chips.removeFromTop (chips.getHeight() - 22.0f * scale);
    auto bpm = current.bpm > 0.0 && crate.hasSample() ? juce::String (juce::roundToInt (current.bpm)) : juce::String ("-");
    auto key = current.key.isNotEmpty() && crate.hasSample() ? current.key : juce::String ("-");
    drawChip (g, chipArea.removeFromLeft (chipWidth), "BPM", bpm, scale);
    chipArea.removeFromLeft (8.0f * scale);
    drawChip (g, chipArea.removeFromLeft (chipWidth), "KEY", key, scale);

    // Gain label
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (12.5f * scale));
    g.drawText ("Gain", gainLabelArea, juce::Justification::centredRight);

    // History header
    g.setColour (colours::panel);
    g.fillRoundedRectangle (historyHeaderArea.getUnion (historyList.getBounds()).toFloat().expanded (0.0f, 2.0f), 8.0f * scale);
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.5f * scale, juce::Font::bold));
    g.drawText ("HISTORY", historyHeaderArea.reduced (juce::roundToInt (12.0f * scale), 0), juce::Justification::centredLeft);
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
    headerRight.removeFromRight (s (14.0f));

    for (auto* b : { &sizeLarge, &sizeMedium, &sizeSmall })
    {
        b->setBounds (headerRight.removeFromRight (s (30.0f)).reduced (0, s (4.0f)));
        headerRight.removeFromRight (s (4.0f));
    }

    headerArea.setRight (headerRight.getRight());
    bounds.removeFromTop (s (8.0f));

    // Filter row
    auto filterRow = bounds.removeFromTop (s (34.0f));
    genreBox.setBounds (filterRow.removeFromLeft (s (200.0f)));
    filterRow.removeFromLeft (s (10.0f));
    decadeBox.setBounds (filterRow.removeFromLeft (s (150.0f)));
    filterRow.removeFromLeft (s (16.0f));
    publicDomainToggle.setBounds (filterRow.removeFromLeft (s (200.0f)));
    bounds.removeFromTop (s (12.0f));

    // Status line
    auto statusArea = bounds.removeFromBottom (s (22.0f));
    bounds.removeFromBottom (s (6.0f));
    statusLabel.setBounds (statusArea);
    statusLabel.setFont (juce::FontOptions (13.0f * scale));

    // History column
    auto history = bounds.removeFromRight (juce::jmax (s (210.0f), bounds.getWidth() * 27 / 100));
    bounds.removeFromRight (s (12.0f));
    historyHeaderArea = history.removeFromTop (s (30.0f));
    historyList.setBounds (history.reduced (s (4.0f), 0).withTrimmedBottom (s (6.0f)));
    historyList.setRowHeight (s (46.0f));

    // Info row: DIG + record card
    auto infoRow = bounds.removeFromTop (s (92.0f));
    digButton.setBounds (infoRow.removeFromLeft (s (120.0f)));
    infoRow.removeFromLeft (s (12.0f));
    infoArea = infoRow;
    archiveLink.setFont (juce::FontOptions (12.0f * scale), false, juce::Justification::centredRight);
    archiveLink.setBounds (infoArea.reduced (s (14.0f), s (10.0f)).removeFromBottom (s (18.0f)).removeFromRight (s (160.0f)));
    bounds.removeFromTop (s (12.0f));

    // Export row
    auto exportRow = bounds.removeFromBottom (s (62.0f));
    saveButton.setBounds (exportRow.removeFromRight (s (120.0f)));
    exportRow.removeFromRight (s (12.0f));
    dragTile.setBounds (exportRow);
    bounds.removeFromBottom (s (10.0f));

    // Transport row
    auto transport = bounds.removeFromBottom (s (32.0f));
    playButton.setBounds (transport.removeFromLeft (s (90.0f)));
    transport.removeFromLeft (s (14.0f));
    loopToggle.setBounds (transport.removeFromLeft (s (80.0f)));
    gainSlider.setBounds (transport.removeFromRight (s (170.0f)));
    gainLabelArea = transport.removeFromRight (s (50.0f)).withTrimmedRight (s (8.0f));
    bounds.removeFromBottom (s (10.0f));

    waveform.setBounds (bounds);

    historyList.updateContent();
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
    auto genreIndex = archive::genres().indexOf (filters.genre);
    genreBox.setSelectedId (genreIndex >= 0 ? genreIndex + anyId + 1 : anyId, juce::dontSendNotification);
    decadeBox.setSelectedId (filters.decade > 0 ? filters.decade : anyId, juce::dontSendNotification);
    publicDomainToggle.setToggleState (filters.publicDomainOnly, juce::dontSendNotification);

    auto& current = crate.getCurrent();
    if (current.audio.get() != shownAudio)
    {
        shownAudio = current.audio.get();
        waveform.setAudio (current.audio, current.sampleRate);
    }

    waveform.setSelection (crate.getSelection());
    waveform.setPlaceholder (crate.isBusy() ? "Digging..." : "No record yet - hit DIG");

    archiveLink.setVisible (current.info.isValid() && crate.hasSample());
    archiveLink.setURL (juce::URL (current.info.pageUrl()));

    dragTile.setEnabled (crate.hasSample());
    saveButton.setEnabled (crate.hasSample());

    if (crate.hasSample())
    {
        auto sel = crate.getEffectiveSelection();
        auto suffix = exporter::regionSuffix (sel, current.audio->getNumSamples(), current.sampleRate);
        dragTile.setSubtitle (exporter::makeBaseName (current.info, current.bpm, current.key) + suffix + ".wav");
    }
    else
    {
        dragTile.setSubtitle ("Dig a record first");
    }

    statusLabel.setText (crate.getStatus(), juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, crate.isStatusError() ? colours::error : colours::textDim);

    historyList.updateContent();
    historyList.repaint();
    repaint();
}

void CrateDigEditor::updateFiltersFromUi()
{
    auto filters = crate.getFilters();
    auto genreId = genreBox.getSelectedId();
    filters.genre = genreId > anyId ? archive::genres()[genreId - anyId - 1] : juce::String();
    filters.decade = decadeBox.getSelectedId() > anyId ? decadeBox.getSelectedId() : 0;
    filters.publicDomainOnly = publicDomainToggle.getToggleState();
    crate.setFilters (filters);

    if (archive::yearRange (filters).isEmpty())
        showMessage ("Public-domain records stop at " + juce::String (archive::publicDomainLastYear) + " - pick an earlier decade", true);
}

void CrateDigEditor::showMessage (const juce::String& text, bool isError)
{
    crate.setStatus (text, isError);
}

void CrateDigEditor::saveWav()
{
    juce::String error;
    auto file = crate.exportSelection (error);

    if (file.existsAsFile())
    {
        showMessage ("Saved " + file.getFileName(), false);
        file.revealToUser();
    }
    else
    {
        showMessage (error.isNotEmpty() ? error : juce::String ("Export failed"), true);
    }
}

void CrateDigEditor::applySizePreset (float factor)
{
    setSize (juce::jlimit (minWidth, maxWidth, juce::roundToInt (defaultWidth * factor)),
             juce::jlimit (minHeight, maxHeight, juce::roundToInt (defaultHeight * factor)));
}

void CrateDigEditor::timerCallback()
{
    auto busy = crate.isBusy();
    digButton.setEnabled (! busy);

    if (busy)
    {
        busyTicks = (busyTicks + 1) % 4;
        digButton.setButtonText ("DIGGING" + juce::String::repeatedString (".", busyTicks));
    }
    else if (digButton.getButtonText() != "DIG")
    {
        digButton.setButtonText ("DIG");
        refreshFromProcessor();
    }

    playButton.setButtonText (crate.isPlaying() ? "Stop" : "Play");
    playButton.setEnabled (crate.hasSample());
}

//==============================================================================
int CrateDigEditor::getNumRows()
{
    return (int) crate.getHistory().size();
}

void CrateDigEditor::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    auto& history = crate.getHistory();

    if (! juce::isPositiveAndBelow (row, (int) history.size()))
        return;

    auto& item = history[(size_t) row];
    auto isCurrent = item.identifier == crate.getCurrent().info.identifier;
    auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (2.0f * scale, 2.0f * scale);

    if (selected || isCurrent)
    {
        g.setColour (isCurrent ? colours::accent.withAlpha (0.15f) : colours::panelRaised);
        g.fillRoundedRectangle (area, 6.0f * scale);
    }

    auto text = area.reduced (10.0f * scale, 5.0f * scale);
    g.setColour (isCurrent ? colours::accent : colours::text);
    g.setFont (juce::FontOptions (13.5f * scale, juce::Font::bold));
    g.drawText (item.title, text.removeFromTop (text.getHeight() * 0.55f), juce::Justification::bottomLeft, true);

    juce::String sub = item.creator.upToFirstOccurrenceOf (",", false, false);

    if (item.year > 0)
        sub << "  -  " << item.year;

    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.5f * scale));
    g.drawText (sub, text, juce::Justification::topLeft, true);
}

void CrateDigEditor::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (! juce::isPositiveAndBelow (row, (int) crate.getHistory().size()))
        return;

    if (crate.getHistory()[(size_t) row].identifier != crate.getCurrent().info.identifier)
        crate.loadFromHistory (row);
}
