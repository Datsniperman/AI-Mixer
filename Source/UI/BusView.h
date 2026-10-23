#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Interprocess/InstanceNode.h"
#include "../DSP/Analyzer.h"

namespace aimixer {

class BusView : public juce::Component, private juce::Timer {
public:
    BusView(juce::AudioProcessorValueTreeState& apvts, InstanceNode& node, AudioAnalyzer& analyzer);
    ~BusView() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::AudioProcessorValueTreeState& apvts;
    InstanceNode& instanceNode;
    AudioAnalyzer& analyzer;

    juce::Label statusLabel;
    float currentRmsDb = -100.0f;
    float currentPeakDb = -100.0f;
};

} // namespace aimixer
