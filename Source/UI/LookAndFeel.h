#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace aimixer {

class AIMixerLookAndFeel : public juce::LookAndFeel_V4 {
public:
    AIMixerLookAndFeel();

    static juce::Colour getDarkBackground() noexcept { return juce::Colour(0xff121417); }
    static juce::Colour getPanelBackground() noexcept { return juce::Colour(0xff1a1d22); }
    static juce::Colour getCardBackground() noexcept { return juce::Colour(0xff22272e); }
    static juce::Colour getBorderColour() noexcept { return juce::Colour(0xff2d333b); }
    static juce::Colour getAccentBlue() noexcept { return juce::Colour(0xff00d2ff); }
    static juce::Colour getAccentGreen() noexcept { return juce::Colour(0xff00e676); }
    static juce::Colour getAccentOrange() noexcept { return juce::Colour(0xffff9100); }
    static juce::Colour getAccentPurple() noexcept { return juce::Colour(0xffb388ff); }
    static juce::Colour getTextPrimary() noexcept { return juce::Colour(0xfff0f6fc); }
    static juce::Colour getTextSecondary() noexcept { return juce::Colour(0xff8b949e); }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                             bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override;
};

} // namespace aimixer
