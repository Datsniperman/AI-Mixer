#include "LookAndFeel.h"

namespace aimixer {

AIMixerLookAndFeel::AIMixerLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, getDarkBackground());
    setColour(juce::ComboBox::backgroundColourId, getCardBackground());
    setColour(juce::ComboBox::textColourId, getTextPrimary());
    setColour(juce::ComboBox::outlineColourId, getBorderColour());
    setColour(juce::PopupMenu::backgroundColourId, getPanelBackground());
    setColour(juce::PopupMenu::textColourId, getTextPrimary());
    setColour(juce::TextButton::buttonColourId, getCardBackground());
    setColour(juce::TextButton::textColourOffId, getTextPrimary());
}

void AIMixerLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                             const juce::Colour& backgroundColour,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown) {
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto baseCol = button.getToggleState() ? getAccentBlue() : backgroundColour;

    if (shouldDrawButtonAsDown) {
        baseCol = baseCol.darker(0.2f);
    } else if (shouldDrawButtonAsHighlighted) {
        baseCol = baseCol.brighter(0.15f);
    }

    g.setColour(baseCol);
    g.fillRoundedRectangle(bounds, 5.0f);

    g.setColour(button.getToggleState() ? getAccentBlue().brighter(0.4f) : getBorderColour());
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
}

void AIMixerLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                      int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                      juce::ComboBox& box) {
    auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);

    g.setColour(getCardBackground());
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(box.hasKeyboardFocus(true) ? getAccentBlue() : getBorderColour());
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    // Draw little arrow on right
    juce::Path arrow;
    float arrowX = width - 16.0f;
    float arrowY = height * 0.5f - 2.0f;
    arrow.addTriangle(arrowX, arrowY, arrowX + 8.0f, arrowY, arrowX + 4.0f, arrowY + 5.0f);
    g.setColour(getTextSecondary());
    g.fillPath(arrow);
}

} // namespace aimixer
