#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace cratedig::ui
{

/** The "drag me into your DAW" handle. The WAV is written when the drag starts. */
class DragTile final : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    DragTile();

    /** Writes (or reuses) the WAV to drag. Return an empty File and set the error to abort. */
    std::function<juce::File (juce::String& error)> prepareFile;
    std::function<void (const juce::String& message, bool isError)> onMessage;

    void setSubtitle (const juce::String& text) { subtitle = text; repaint(); }
    void setScale (float newScale) { scale = newScale; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void enablementChanged() override { repaint(); }

private:
    juce::String subtitle;
    bool dragInProgress = false;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DragTile)
};

} // namespace cratedig::ui
