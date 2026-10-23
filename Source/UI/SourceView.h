#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Interprocess/InstanceNode.h"
#include "../DSP/Analyzer.h"
#include "../DSP/SafeChain.h"
#include "../Analysis/TrackNameParser.h"

namespace aimixer {

class AIMixerAudioProcessor;

class SourceView : public juce::Component, private juce::Timer {
public:
    explicit SourceView(AIMixerAudioProcessor& processor);
    ~SourceView() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawSpectrum(juce::Graphics& g, juce::Rectangle<float> area);
    void drawMeters(juce::Graphics& g, juce::Rectangle<float> area);
    void drawSafeChainStrip(juce::Graphics& g, juce::Rectangle<float> area);

    AIMixerAudioProcessor& processor;

    // Instrument controls
    juce::ComboBox instrumentBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> instrumentAttachment;

    // Safe Chain Controls
    juce::ToggleButton autoTrimToggle { "Auto-Trim" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoTrimAttachment;

    juce::Slider manualTrimSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> manualTrimAttachment;

    juce::Slider hpfSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hpfAttachment;
    juce::TextButton hpfResetButton { "Auto" };

    juce::Slider lpfSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpfAttachment;
    juce::TextButton lpfResetButton { "Auto" };

    juce::ToggleButton compToggle { "Leveller" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> compAttachment;

    juce::Slider compThreshSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compThreshAttachment;

    juce::Label statusLabel;
    juce::Label coordinatorBadge;
    juce::Label identitySourceLabel;

    // Solver Section Controls
    juce::ToggleButton solverToggle { "Mix Solver" };
    juce::Label solverReadoutLabel;
    juce::Label solverReasonLabel1;
    juce::Label solverReasonLabel2;
    juce::Label solverReasonLabel3;

    // MVP 5 Controls
    juce::Slider moreLessSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> moreLessAttachment;
    juce::Label moreLessLabel { {}, "More / Less" };

    juce::ToggleButton localTailToggle { "Local Tail" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> localTailAttachment;

    juce::Slider sendTrimSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sendTrimAttachment;
    juce::Label sendTrimLabel { {}, "Send Trim" };

    juce::Label sessionModePill;

    float currentSpectrum[NUM_SPECTRUM_BANDS] = {0.0f};
    float currentRmsDb = -100.0f;
    float currentPeakDb = -100.0f;
    int timerTickCount = 0;
};

} // namespace aimixer
