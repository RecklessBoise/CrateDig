#include "DragTile.h"

#include "CrateLookAndFeel.h"

namespace cratedig::ui
{

DragTile::DragTile()
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Drag this into a track of your DAW");
}

void DragTile::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    auto active = isEnabled();
    auto hot = active && (isMouseOver() || dragInProgress);
    auto corner = 8.0f * scale;

    g.setColour (hot ? colours::accent.withAlpha (0.16f) : colours::panel);
    g.fillRoundedRectangle (bounds, corner);

    juce::Path outline;
    outline.addRoundedRectangle (bounds, corner);
    juce::Path dashed;
    const float dashes[] { 6.0f * scale, 4.0f * scale };
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (active ? colours::accent.withAlpha (hot ? 1.0f : 0.7f) : colours::outline);
    g.fillPath (dashed);

    // Icon: a small record with a down arrow.
    auto iconSize = juce::jmin (bounds.getHeight() * 0.55f, 34.0f * scale);
    auto icon = juce::Rectangle<float> (iconSize, iconSize).withCentre ({ bounds.getX() + 14.0f * scale + iconSize * 0.5f, bounds.getCentreY() });
    g.setColour (active ? colours::accent : colours::textDim);
    g.drawEllipse (icon, 1.6f * scale);
    g.fillEllipse (icon.withSizeKeepingCentre (iconSize * 0.28f, iconSize * 0.28f));

    auto textArea = bounds.withTrimmedLeft (iconSize + 26.0f * scale).reduced (0.0f, 6.0f * scale);
    g.setColour (active ? colours::text : colours::textDim);
    g.setFont (juce::FontOptions (15.0f * scale, juce::Font::bold));
    g.drawText ("DRAG TO DAW", textArea.removeFromTop (textArea.getHeight() * 0.5f), juce::Justification::bottomLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (12.0f * scale));
    g.drawText (subtitle, textArea, juce::Justification::topLeft, true);
}

void DragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragInProgress || ! isEnabled() || e.getDistanceFromDragStart() < 6 || ! prepareFile)
        return;

    juce::String error;
    auto file = prepareFile (error);

    if (! file.existsAsFile())
    {
        if (onMessage)
            onMessage (error.isNotEmpty() ? error : juce::String ("Export failed"), true);

        return;
    }

    dragInProgress = true;
    repaint();

    juce::Component::SafePointer<DragTile> safeThis (this);
    auto started = juce::DragAndDropContainer::performExternalDragDropOfFiles (
        { file.getFullPathName() }, false, this, [safeThis]
        {
            if (safeThis != nullptr)
            {
                safeThis->dragInProgress = false;
                safeThis->repaint();
            }
        });

    if (! started)
    {
        dragInProgress = false;
        repaint();
    }

    if (onMessage)
        onMessage (started ? "Exported " + file.getFileName() : juce::String ("Your host refused the drag"), ! started);
}

} // namespace cratedig::ui
