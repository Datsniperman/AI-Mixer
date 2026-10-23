#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../DSP/HostedPluginChain.h"
#include "../Brain/PluginCurator.h"
namespace aimixer { class AIMixerAudioProcessor; }

namespace aimixer {

class WavesRackComponent : public juce::Component, private juce::Timer {
public:
    explicit WavesRackComponent(HostedPluginChain& chain);
    ~WavesRackComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateDisplay();

private:
    void timerCallback() override;

    struct SlotUI {
        juce::Label roleLabel;
        juce::ComboBox pluginSelector;
        juce::TextButton openGuiButton { "GUI" };
        juce::ToggleButton bypassToggle { "Bypass" };
        juce::TextButton clearButton { "X" };
    };

    HostedPluginChain& chain;
    juce::ToggleButton masterBypassToggle { "Waves Rack" };
        juce::TextButton autoCurateButton { "Auto-Curate Chain" };
    AIMixerAudioProcessor* processor = nullptr;
public:
    void setProcessor(AIMixerAudioProcessor* p) { processor = p; }
    juce::Label latencyLabel;

    std::vector<std::unique_ptr<SlotUI>> slotUIs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavesRackComponent)
};

} // namespace aimixer


