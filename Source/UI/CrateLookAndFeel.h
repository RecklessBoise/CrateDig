#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cratedig::ui
{

namespace colours
{
    inline const juce::Colour background { 0xff13110f };
    inline const juce::Colour panel { 0xff1d1a17 };
    inline const juce::Colour panelRaised { 0xff27231f };
    inline const juce::Colour outline { 0xff3a342d };
    inline const juce::Colour text { 0xffede6dc };
    inline const juce::Colour textDim { 0xff9c9288 };
    inline const juce::Colour accent { 0xfff2a33a };
    inline const juce::Colour accentDark { 0xffb8741f };
    inline const juce::Colour error { 0xffe5684f };
} // namespace colours

/** Dark "record crate" look. `scale` tracks the editor size so text grows with the window. */
class CrateLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CrateLookAndFeel();

    void setScale (float newScale) { scale = newScale; }
    float getScale() const { return scale; }

    juce::Font font (float size, bool bold = false) const;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool isMouseOverButton, bool isButtonDown) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

private:
    float scale = 1.0f;
};

} // namespace cratedig::ui
