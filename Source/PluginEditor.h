#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/MasterMixView.h"
#include "UI/SourceView.h"
#include "UI/BusView.h"

namespace aimixer {

class AIMixerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer {
public:
    explicit AIMixerAudioProcessorEditor(AIMixerAudioProcessor&);
    ~AIMixerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void updateActiveView();

    bool hasLoggedFirstResized = false;
    int paintCount = 0;

    AIMixerAudioProcessor& audioProcessor;
    AIMixerLookAndFeel customLookAndFeel;

    // Header components
    juce::Label titleLabel;
    juce::TextButton sourceRoleButton { "Source" };
    juce::TextButton busRoleButton { "Bus" };
    juce::TextButton masterRoleButton { "Master" };

    juce::ComboBox destinationBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> destinationAttachment;

    juce::Label coordinatorStatusBadge;

    // View components
    std::unique_ptr<SourceView> sourceView;
    std::unique_ptr<BusView> busView;
    std::unique_ptr<MasterMixView> masterMixView;

    PluginRole activeViewRole = PluginRole::Source;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIMixerAudioProcessorEditor)
};

} // namespace aimixer
