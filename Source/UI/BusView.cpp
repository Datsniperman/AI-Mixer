#include "BusView.h"
#include "LookAndFeel.h"

namespace aimixer {

BusView::BusView(juce::AudioProcessorValueTreeState& vts, InstanceNode& node, AudioAnalyzer& an)
    : apvts(vts), instanceNode(node), analyzer(an) {
    statusLabel.setText("Bus Role: Group coordination & glue compression (MVP Step 4).", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getTextSecondary());
    statusLabel.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(statusLabel);

    startTimerHz(30);
}

BusView::~BusView() {
    stopTimer();
}

void BusView::paint(juce::Graphics& g) {
    g.fillAll(AIMixerLookAndFeel::getDarkBackground());

    auto bounds = getLocalBounds().reduced(20);
    auto topArea = bounds.removeFromTop(40);

    g.setColour(AIMixerLookAndFeel::getTextPrimary());
    g.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    g.drawText("GROUP BUS STRIP", topArea.removeFromLeft(200), juce::Justification::centredLeft);

    auto card = bounds.reduced(10);
    g.setColour(AIMixerLookAndFeel::getCardBackground());
    g.fillRoundedRectangle(card.toFloat(), 6.0f);
    g.setColour(AIMixerLookAndFeel::getAccentPurple());
    g.drawRoundedRectangle(card.toFloat(), 6.0f, 1.0f);

    g.setColour(AIMixerLookAndFeel::getTextPrimary());
    g.setFont(juce::FontOptions(14.0f));
    juce::String busStats = "Bus Meters: RMS " + juce::String(currentRmsDb, 1) + " dBFS | Peak " + juce::String(currentPeakDb, 1) + " dB";
    g.drawText(busStats, card.reduced(20), juce::Justification::centred);
}

void BusView::resized() {
    auto bounds = getLocalBounds().reduced(20);
    statusLabel.setBounds(bounds.removeFromBottom(30));
}

void BusView::timerCallback() {
    currentRmsDb = analyzer.getInstantRmsDb();
    currentPeakDb = analyzer.getInstantPeakDb();
    repaint();
}

} // namespace aimixer
