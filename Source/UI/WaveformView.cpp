#include "WaveformView.h"

#include "CrateLookAndFeel.h"

namespace cratedig::ui
{

WaveformView::WaveformView()
{
    setRepaintsOnMouseActivity (false);
    startTimerHz (30);
}

void WaveformView::setAudio (std::shared_ptr<const juce::AudioBuffer<float>> newAudio, double newSampleRate)
{
    audio = std::move (newAudio);
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    totalSamples = audio != nullptr ? audio->getNumSamples() : 0;
    peaks.assign (numBins, { 0.0f, 0.0f });

    if (totalSamples > 0)
    {
        for (int bin = 0; bin < numBins; ++bin)
        {
            auto start = (int) ((juce::int64) bin * totalSamples / numBins);
            auto end = juce::jmax (start + 1, (int) ((juce::int64) (bin + 1) * totalSamples / numBins));
            end = juce::jmin (end, totalSamples);
            auto lo = 0.0f, hi = 0.0f;

            for (int ch = 0; ch < audio->getNumChannels(); ++ch)
            {
                auto range = juce::FloatVectorOperations::findMinAndMax (audio->getReadPointer (ch, start), end - start);
                lo = juce::jmin (lo, range.getStart());
                hi = juce::jmax (hi, range.getEnd());
            }

            peaks[(size_t) bin] = { lo, hi };
        }
    }

    repaint();
}

void WaveformView::setSelection (juce::Range<int> newSelection)
{
    if (selection != newSelection)
    {
        selection = newSelection;
        repaint();
    }
}

juce::Rectangle<float> WaveformView::waveArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f * scale, 8.0f * scale).withTrimmedBottom (16.0f * scale);
}

int WaveformView::xToSample (float x) const
{
    auto area = waveArea();
    auto proportion = juce::jlimit (0.0f, 1.0f, (x - area.getX()) / juce::jmax (1.0f, area.getWidth()));
    return (int) (proportion * (float) totalSamples);
}

float WaveformView::sampleToX (double sample) const
{
    auto area = waveArea();
    return area.getX() + (float) (sample / juce::jmax (1, totalSamples)) * area.getWidth();
}

juce::String WaveformView::formatTime (double seconds) const
{
    auto mins = (int) seconds / 60;
    auto secs = seconds - mins * 60;
    return juce::String (mins) + ":" + juce::String (secs, 1).paddedLeft ('0', 4);
}

void WaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 8.0f * scale);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f * scale, 1.0f);

    auto area = waveArea();

    if (totalSamples == 0)
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (15.0f * scale));
        g.drawText (placeholder, area, juce::Justification::centred);
        return;
    }

    auto hasSelection = selection.getLength() > 1;
    auto centreY = area.getCentreY();
    auto halfHeight = area.getHeight() * 0.5f;
    auto width = (int) area.getWidth();

    if (hasSelection)
    {
        auto x1 = sampleToX (selection.getStart()), x2 = sampleToX (selection.getEnd());
        g.setColour (colours::accent.withAlpha (0.14f));
        g.fillRect (juce::Rectangle<float> (x1, area.getY(), x2 - x1, area.getHeight()));
    }

    g.setColour (colours::outline);
    g.drawHorizontalLine ((int) centreY, area.getX(), area.getRight());

    for (int px = 0; px < width; ++px)
    {
        auto binStart = (int) ((juce::int64) px * numBins / width);
        auto binEnd = juce::jmax (binStart + 1, (int) ((juce::int64) (px + 1) * numBins / width));
        auto lo = 0.0f, hi = 0.0f;

        for (int b = binStart; b < juce::jmin (binEnd, numBins); ++b)
        {
            lo = juce::jmin (lo, peaks[(size_t) b].first);
            hi = juce::jmax (hi, peaks[(size_t) b].second);
        }

        auto x = area.getX() + (float) px;
        auto sample = (int) ((juce::int64) px * totalSamples / width);
        auto inside = ! hasSelection || selection.contains (sample);

        g.setColour (inside ? colours::accent : colours::accent.withAlpha (0.35f));
        g.drawVerticalLine ((int) x, centreY - hi * halfHeight, centreY - lo * halfHeight + 1.0f);
    }

    if (hasSelection)
    {
        g.setColour (colours::text);

        for (auto edge : { selection.getStart(), selection.getEnd() })
        {
            auto x = sampleToX (edge);
            g.fillRect (juce::Rectangle<float> (x - 1.0f, area.getY(), 2.0f, area.getHeight()));
        }
    }

    if (auto playhead = getPlayhead ? getPlayhead() : -1.0; playhead >= 0.0)
    {
        g.setColour (juce::Colours::white);
        g.fillRect (juce::Rectangle<float> (sampleToX (playhead) - 0.75f, area.getY(), 1.5f, area.getHeight()));
    }

    // Time ruler
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (11.0f * scale));
    auto ruler = juce::Rectangle<float> (area.getX(), area.getBottom() + 2.0f, area.getWidth(), 14.0f * scale);
    auto duration = totalSamples / sampleRate;
    g.drawText ("0:00.0", ruler, juce::Justification::centredLeft);
    g.drawText (formatTime (duration), ruler, juce::Justification::centredRight);

    if (hasSelection)
    {
        auto label = formatTime (selection.getStart() / sampleRate) + " - " + formatTime (selection.getEnd() / sampleRate)
                   + "  (" + juce::String (selection.getLength() / sampleRate, 2) + "s)";
        g.setColour (colours::accent);
        g.drawText (label, ruler, juce::Justification::centred);
    }
    else
    {
        g.drawText ("drag to select a chop", ruler, juce::Justification::centred);
    }
}

void WaveformView::mouseMove (const juce::MouseEvent& e)
{
    if (selection.getLength() > 1)
    {
        auto nearEdge = std::abs (e.position.x - sampleToX (selection.getStart())) < 6.0f
                     || std::abs (e.position.x - sampleToX (selection.getEnd())) < 6.0f;
        setMouseCursor (nearEdge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::IBeamCursor);
    }
    else
    {
        setMouseCursor (juce::MouseCursor::IBeamCursor);
    }
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    dragged = false;

    if (totalSamples == 0)
        return;

    if (selection.getLength() > 1 && std::abs (e.position.x - sampleToX (selection.getStart())) < 6.0f)
    {
        dragMode = DragMode::startEdge;
        dragAnchor = selection.getEnd();
    }
    else if (selection.getLength() > 1 && std::abs (e.position.x - sampleToX (selection.getEnd())) < 6.0f)
    {
        dragMode = DragMode::endEdge;
        dragAnchor = selection.getStart();
    }
    else
    {
        dragMode = DragMode::create;
        dragAnchor = xToSample (e.position.x);
    }
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::none || totalSamples == 0)
        return;

    if (! dragged && e.getDistanceFromDragStart() < 4)
        return;

    dragged = true;
    auto pos = xToSample (e.position.x);
    auto newSelection = juce::Range<int> (juce::jmin (pos, dragAnchor), juce::jmax (pos, dragAnchor));
    setSelection (newSelection);

    if (onSelectionChanged)
        onSelectionChanged (newSelection);
}

void WaveformView::mouseUp (const juce::MouseEvent& e)
{
    if (totalSamples > 0 && ! dragged && dragMode == DragMode::create && onClick)
        onClick (xToSample (e.position.x));

    dragMode = DragMode::none;
}

void WaveformView::mouseDoubleClick (const juce::MouseEvent&)
{
    setSelection ({});

    if (onSelectionChanged)
        onSelectionChanged ({});
}

void WaveformView::timerCallback()
{
    auto playhead = getPlayhead ? getPlayhead() : -1.0;

    if (std::abs (playhead - lastPlayhead) > 0.5)
    {
        lastPlayhead = playhead;
        repaint();
    }
}

} // namespace cratedig::ui
