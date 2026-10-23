#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common/SharedProtocol.h"
#include "../Interprocess/InstanceNode.h"
#include <vector>

namespace aimixer {

class AIMixerAudioProcessor;

// ==============================================================================
// Live Tile/Card for a Single Instance
// ==============================================================================
class InstanceTileComponent : public juce::Component {
public:
    InstanceTileComponent();
    void updateData(const InstanceTelemetry& data);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void drawMiniSpectrum(juce::Graphics& g, juce::Rectangle<float> area);
    void drawMeters(juce::Graphics& g, juce::Rectangle<float> area);

    InstanceTelemetry telemetry;
    juce::String trackTitle = "Track";
    juce::String instrumentTitle = "Unknown";
    bool hasData = false;
};

// ==============================================================================
// Master Mix View: Live Grid of all instances
// ==============================================================================
class MasterMixView : public juce::Component, private juce::Timer {
public:
    explicit MasterMixView(AIMixerAudioProcessor& proc);
    ~MasterMixView() override;

    size_t getTileCount() const noexcept { return tiles.size(); }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshInstanceTiles();

    AIMixerAudioProcessor& processor;
    InstanceNode& instanceNode;
    juce::ToggleButton globalSolverToggle { "Global Solver: ON" };

    // MVP 5 Taste Panel Controls
    juce::ComboBox genreBox;
    juce::Slider followSlider;
    juce::Label followLabel { {}, "Follow" };

    juce::Slider macroVocalForwardSlider;
    juce::Label macroVocalForwardLabel { {}, "Vocal" };

    juce::Slider macroEnergySlider;
    juce::Label macroEnergyLabel { {}, "Punch" };

    juce::Slider macroWarmBrightSlider;
    juce::Label macroWarmBrightLabel { {}, "Tone" };

    juce::Slider macroSpaceSlider;
    juce::Label macroSpaceLabel { {}, "Space" };

    juce::Slider macroSaturationSlider;
    juce::Label macroSaturationLabel { {}, "Drive" };

    juce::ComboBox sessionModeBox;
    juce::Label learnedStatusLabel;
    juce::TextButton resetLearnedButton { "Reset Profile" };

    juce::Viewport viewport;
    juce::Component gridContainer;
    std::vector<std::unique_ptr<InstanceTileComponent>> tiles;

    int lastInstanceCount = -1;
    int timerTickCount = 0;
};

} // namespace aimixer
