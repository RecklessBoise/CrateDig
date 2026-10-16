#include "CrateLookAndFeel.h"

namespace cratedig::ui
{

CrateLookAndFeel::CrateLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::TextButton::buttonColourId, colours::panelRaised);
    setColour (juce::TextButton::buttonOnColourId, colours::accent);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::background);
    setColour (juce::ComboBox::backgroundColourId, colours::panelRaised);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, colours::outline);
    setColour (juce::ComboBox::arrowColourId, colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, colours::panelRaised);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::background);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ToggleButton::textColourId, colours::text);
    setColour (juce::ToggleButton::tickColourId, colours::accent);
    setColour (juce::Slider::thumbColourId, colours::accent);
    setColour (juce::Slider::trackColourId, colours::accent);
    setColour (juce::Slider::backgroundColourId, colours::outline);
    setColour (juce::ListBox::backgroundColourId, colours::panel);
    setColour (juce::ScrollBar::thumbColourId, colours::outline);
    setColour (juce::HyperlinkButton::textColourId, colours::accent);
}

juce::Font CrateLookAndFeel::font (float size, bool bold) const
{
    return juce::Font (juce::FontOptions (size * scale, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font CrateLookAndFeel::getTextButtonFont (juce::TextButton& button, int buttonHeight)
{
    if (button.getComponentID() == "dig")
        return juce::Font (juce::FontOptions (juce::jmin (26.0f * scale, (float) buttonHeight * 0.4f), juce::Font::bold));

    return juce::Font (juce::FontOptions (juce::jmin (15.0f * scale, (float) buttonHeight * 0.5f), juce::Font::bold));
}

juce::Font CrateLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return juce::Font (juce::FontOptions (juce::jmin (14.0f * scale, (float) box.getHeight() * 0.5f)));
}

juce::Font CrateLookAndFeel::getPopupMenuFont()
{
    return font (14.0f);
}

juce::Font CrateLookAndFeel::getLabelFont (juce::Label& label)
{
    return label.getFont();
}

void CrateLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                             bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto corner = 6.0f * scale;
    auto base = button.getToggleState() ? button.findColour (juce::TextButton::buttonOnColourId) : backgroundColour;

    if (! button.isEnabled())
        base = base.withMultipliedAlpha (0.4f);
    else if (isButtonDown)
        base = base.darker (0.2f);
    else if (isMouseOverButton)
        base = base.brighter (0.12f);

    g.setColour (base);
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (colours::outline.withAlpha (button.getToggleState() ? 0.0f : 1.0f));
    g.drawRoundedRectangle (bounds, corner, 1.0f);
}

void CrateLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    auto corner = 6.0f * scale;

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (box.hasKeyboardFocus (true) ? colours::accent : colours::outline);
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    auto arrowSize = (float) height * 0.22f;
    auto centre = juce::Point<float> ((float) width - (float) height * 0.5f, (float) height * 0.5f);
    juce::Path arrow;
    arrow.addTriangle (centre.x - arrowSize, centre.y - arrowSize * 0.5f,
                       centre.x + arrowSize, centre.y - arrowSize * 0.5f,
                       centre.x, centre.y + arrowSize * 0.6f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void CrateLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool isMouseOverButton, bool)
{
    auto bounds = button.getLocalBounds().toFloat();
    auto boxSize = juce::jmin (bounds.getHeight() * 0.6f, 18.0f * scale);
    auto box = juce::Rectangle<float> (boxSize, boxSize).withCentre ({ bounds.getX() + boxSize * 0.5f + 2.0f, bounds.getCentreY() });

    g.setColour (button.getToggleState() ? colours::accent : colours::panelRaised);
    g.fillRoundedRectangle (box, 4.0f * scale);
    g.setColour (isMouseOverButton ? colours::accent : colours::outline);
    g.drawRoundedRectangle (box, 4.0f * scale, 1.0f);

    if (button.getToggleState())
    {
        juce::Path tick;
        tick.startNewSubPath (box.getX() + box.getWidth() * 0.22f, box.getCentreY());
        tick.lineTo (box.getX() + box.getWidth() * 0.43f, box.getBottom() - box.getHeight() * 0.25f);
        tick.lineTo (box.getRight() - box.getWidth() * 0.2f, box.getY() + box.getHeight() * 0.25f);
        g.setColour (colours::background);
        g.strokePath (tick, juce::PathStrokeType (2.0f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (button.findColour (juce::ToggleButton::textColourId).withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
    g.setFont (font (13.5f));
    g.drawFittedText (button.getButtonText(),
                      bounds.withTrimmedLeft (boxSize + 10.0f * scale).toNearestInt(),
                      juce::Justification::centredLeft, 1);
}

void CrateLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }

    auto trackHeight = 4.0f * scale;
    auto track = juce::Rectangle<float> ((float) x, (float) y + (float) height * 0.5f - trackHeight * 0.5f, (float) width, trackHeight);

    g.setColour (colours::outline);
    g.fillRoundedRectangle (track, trackHeight * 0.5f);
    g.setColour (colours::accent);
    g.fillRoundedRectangle (track.withRight (sliderPos), trackHeight * 0.5f);

    auto thumb = 12.0f * scale;
    g.setColour (colours::text);
    g.fillEllipse (juce::Rectangle<float> (thumb, thumb).withCentre ({ sliderPos, track.getCentreY() }));
}

} // namespace cratedig::ui
