#include "MasterMixView.h"
#include "LookAndFeel.h"
#include "../PluginProcessor.h"
#include "../Common/LoadTrace.h"
#include <iomanip>
#include <sstream>

namespace aimixer {

// ==============================================================================
// InstanceTileComponent Implementation
// ==============================================================================
InstanceTileComponent::InstanceTileComponent() {
    setOpaque(false);
}

void InstanceTileComponent::updateData(const InstanceTelemetry& data) {
    telemetry = data;
    trackTitle = juce::String(data.trackName);
    if (trackTitle.isEmpty()) {
        trackTitle = "Track " + juce::String(data.instanceId % 100);
    }
    if (data.role == PluginRole::Bus) {
        instrumentTitle = juce::String(getBusTypeName(data.busType)) + " Bus";
    } else if (data.role == PluginRole::Master) {
        instrumentTitle = juce::String(getDestinationName(data.destination)) + " Master";
    } else {
        instrumentTitle = getInstrumentName(data.instrumentType);
    }
    hasData = true;
    repaint();
}

void InstanceTileComponent::resized() {}

void InstanceTileComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    // Card background
    g.setColour(AIMixerLookAndFeel::getCardBackground());
    g.fillRoundedRectangle(bounds, 6.0f);

    // Border: Highlight gold if coordinator, blue if active source, purple if bus
    juce::Colour borderCol = AIMixerLookAndFeel::getBorderColour();
    if (telemetry.isCoordinator) {
        borderCol = AIMixerLookAndFeel::getAccentOrange();
    } else if (telemetry.role == PluginRole::Master) {
        borderCol = AIMixerLookAndFeel::getAccentOrange().withAlpha(0.6f);
    } else if (telemetry.role == PluginRole::Bus) {
        borderCol = AIMixerLookAndFeel::getAccentPurple();
    } else if (telemetry.signalPresent) {
        borderCol = AIMixerLookAndFeel::getAccentBlue().withAlpha(0.7f);
    }

    g.setColour(borderCol);
    g.drawRoundedRectangle(bounds, 6.0f, telemetry.isCoordinator ? 2.0f : 1.0f);

    if (!hasData) return;

    // Top Header area
    auto headerArea = bounds.removeFromTop(28.0f).reduced(8.0f, 4.0f);
    
    // Role Badge
    auto rolePill = headerArea.removeFromRight(56.0f);
    juce::Colour roleCol = (telemetry.role == PluginRole::Master) ? AIMixerLookAndFeel::getAccentOrange() :
                           (telemetry.role == PluginRole::Bus)    ? AIMixerLookAndFeel::getAccentPurple() :
                                                                    AIMixerLookAndFeel::getAccentBlue();
    g.setColour(roleCol.withAlpha(0.2f));
    g.fillRoundedRectangle(rolePill, 3.0f);
    g.setColour(roleCol);
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawText(getRoleName(telemetry.role), rolePill, juce::Justification::centred);

    // Track Name
    g.setColour(AIMixerLookAndFeel::getTextPrimary());
    g.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    g.drawText(trackTitle, headerArea, juce::Justification::centredLeft, true);

    // Subheader: Instrument & Coordinator Crown
    auto subHeaderArea = bounds.removeFromTop(18.0f).reduced(8.0f, 0.0f);
    if (telemetry.isCoordinator) {
        auto coordBadge = subHeaderArea.removeFromRight(80.0f);
        g.setColour(AIMixerLookAndFeel::getAccentOrange());
        g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
        g.drawText("* COORDINATOR", coordBadge, juce::Justification::centredRight);
    }

    g.setColour(AIMixerLookAndFeel::getTextSecondary());
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(instrumentTitle, subHeaderArea, juce::Justification::centredLeft, true);

    bounds.removeFromTop(4.0f);

    // Center area: Meters + 32-band Spectrum
    auto visualArea = bounds.removeFromTop(70.0f).reduced(8.0f, 2.0f);
    auto meterArea = visualArea.removeFromRight(24.0f);
    visualArea.removeFromRight(6.0f); // Gap

    drawMiniSpectrum(g, visualArea);
    drawMeters(g, meterArea);

    // Bottom Status Line
    auto statusArea = bounds.reduced(8.0f, 2.0f);

    // Right side: Solver Info (Bus: "Solver: n/a (glue)", Master: "LUFS | Limiter", Other: Ride + Dot if cuts/duck active)
    if (telemetry.role == PluginRole::Bus) {
        auto busLabelArea = statusArea.removeFromRight(135.0f);
        g.setColour(AIMixerLookAndFeel::getTextSecondary().withAlpha(0.85f));
        g.setFont(juce::FontOptions(10.0f));
        juce::String glueStr = juce::String(telemetry.currentCompGrDb, 1) + " dB glue";
        g.drawText("Solver: n/a (" + glueStr + ")", busLabelArea, juce::Justification::centredRight);
    } else if (telemetry.role == PluginRole::Master) {
        auto masterLabelArea = statusArea.removeFromRight(145.0f);
        g.setColour(AIMixerLookAndFeel::getAccentOrange().withAlpha(0.9f));
        g.setFont(juce::FontOptions(10.0f));
        juce::String masterStr = juce::String(telemetry.integratedLufs, 1) + " LUFS | Lim: " + juce::String(telemetry.limiterGrDb, 1) + " dB";
        g.drawText(masterStr, masterLabelArea, juce::Justification::centredRight);
    } else {
        auto solverArea = statusArea.removeFromRight(95.0f);
        if (telemetry.solverCutsActive || telemetry.solverDuckActive) {
            auto dotArea = solverArea.removeFromLeft(10.0f);
            g.setColour(AIMixerLookAndFeel::getAccentOrange());
            g.fillEllipse(dotArea.getCentreX() - 3.0f, dotArea.getCentreY() - 3.0f, 6.0f, 6.0f);
        }
        float ride = telemetry.currentRideOffsetDb;
        juce::String rideStr = (ride >= 0.0f ? "+" : "") + juce::String(ride, 1) + " dB";
        g.setColour(std::abs(ride) > 0.05f ? AIMixerLookAndFeel::getAccentBlue() : AIMixerLookAndFeel::getTextSecondary());
        g.setFont(juce::FontOptions(10.0f).withStyle(std::abs(ride) > 0.05f ? "Bold" : "Regular"));
        g.drawText(rideStr, solverArea, juce::Justification::centredRight);
    }

    g.setColour(AIMixerLookAndFeel::getTextSecondary());
    g.setFont(juce::FontOptions(10.0f));
    juce::String statusText = "RMS: " + juce::String(telemetry.rmsDb, 1) + " dBFS  |  Peak: " + juce::String(telemetry.peakDb, 1) + " dB";
    g.drawText(statusText, statusArea, juce::Justification::centredLeft);
}

void InstanceTileComponent::drawMiniSpectrum(juce::Graphics& g, juce::Rectangle<float> area) {
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(AIMixerLookAndFeel::getBorderColour().withAlpha(0.5f));
    g.drawRoundedRectangle(area, 4.0f, 0.5f);

    const float barSpacing = 1.0f;
    const float totalSpacing = barSpacing * (NUM_SPECTRUM_BANDS - 1);
    const float barWidth = std::max(1.0f, (area.getWidth() - totalSpacing) / static_cast<float>(NUM_SPECTRUM_BANDS));

    juce::ColourGradient grad(
        AIMixerLookAndFeel::getAccentBlue(), area.getX(), area.getBottom(),
        AIMixerLookAndFeel::getAccentGreen(), area.getX(), area.getY(), false
    );
    g.setGradientFill(grad);

    for (size_t b = 0; b < NUM_SPECTRUM_BANDS; ++b) {
        float normalized = std::clamp(telemetry.spectrumBands[b], 0.0f, 1.0f);
        float barHeight = normalized * (area.getHeight() - 4.0f);
        float x = area.getX() + b * (barWidth + barSpacing);
        float y = area.getBottom() - 2.0f - barHeight;

        g.fillRect(x, y, barWidth, std::max(1.0f, barHeight));
    }
}

void InstanceTileComponent::drawMeters(juce::Graphics& g, juce::Rectangle<float> area) {
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(area, 3.0f);

    // Map -60 dB to 0 dB -> 0.0 to 1.0
    float normRms = std::clamp((telemetry.rmsDb + 60.0f) / 60.0f, 0.0f, 1.0f);
    float normPeak = std::clamp((telemetry.peakDb + 60.0f) / 60.0f, 0.0f, 1.0f);

    // RMS Bar
    float rmsHeight = normRms * area.getHeight();
    auto rmsBar = area.removeFromLeft(area.getWidth() * 0.55f);
    
    juce::Colour rmsColor = (telemetry.rmsDb > -6.0f) ? AIMixerLookAndFeel::getAccentOrange() :
                            (telemetry.rmsDb > -18.0f) ? AIMixerLookAndFeel::getAccentGreen() :
                                                         AIMixerLookAndFeel::getAccentBlue();
    g.setColour(rmsColor);
    g.fillRect(rmsBar.getX() + 1.0f, area.getBottom() - rmsHeight, rmsBar.getWidth() - 2.0f, rmsHeight);

    // Peak Indicator Line
    float peakY = area.getBottom() - (normPeak * area.getHeight());
    g.setColour(juce::Colours::white);
    g.fillRect(area.getX(), peakY, area.getWidth(), 1.5f);
}

// ==============================================================================
// MasterMixView Implementation
// ==============================================================================
MasterMixView::MasterMixView(AIMixerAudioProcessor& proc)
    : processor(proc), instanceNode(proc.getInstanceNode()) {
    
    globalSolverToggle.setToggleState(processor.isGlobalSolverEnabled(), juce::dontSendNotification);
    globalSolverToggle.onClick = [this]() {
        bool on = globalSolverToggle.getToggleState();
        globalSolverToggle.setButtonText(on ? "Global Solver: ON" : "Global Solver: OFF");
        processor.setGlobalSolverEnabled(on);
    };
    addAndMakeVisible(globalSolverToggle);

    // MVP 5: Taste Controls Setup
    auto profiles = processor.getProfileManager().getAllProfiles();
    for (const auto& p : profiles) {
        genreBox.addItem(p.name, static_cast<int>(p.profileId));
    }
    genreBox.setSelectedId(1, juce::dontSendNotification);
    genreBox.onChange = [this]() {
        int id = genreBox.getSelectedId();
        if (id > 0) processor.getProfileManager().setActiveProfile(static_cast<uint32_t>(id));
    };
    addAndMakeVisible(genreBox);

    followSlider.setRange(0.0, 100.0, 1.0);
    followSlider.setValue(100.0, juce::dontSendNotification);
    followSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    followSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 16);
    followSlider.setTextValueSuffix("%");
    followSlider.onValueChange = [this]() {
        processor.getProfileManager().setFollowPercent(static_cast<float>(followSlider.getValue()));
    };
    addAndMakeVisible(followSlider);
    followLabel.setFont(juce::FontOptions(10.0f));
    followLabel.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getTextSecondary());
    addAndMakeVisible(followLabel);

    auto setupMacro = [this](juce::Slider& s, juce::Label& l, const char* name, std::function<void(float)> cb) {
        s.setRange(-100.0, 100.0, 1.0);
        s.setValue(0.0, juce::dontSendNotification);
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.onValueChange = [cb, &s]() { cb(static_cast<float>(s.getValue())); };
        addAndMakeVisible(s);

        l.setText(name, juce::dontSendNotification);
        l.setFont(juce::FontOptions(10.0f));
        l.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getTextSecondary());
        addAndMakeVisible(l);
    };

    setupMacro(macroVocalForwardSlider, macroVocalForwardLabel, "Vocal", [this](float v) {
        processor.getProfileManager().setMacroVocalForward(v);
    });
    setupMacro(macroEnergySlider, macroEnergyLabel, "Punch", [this](float v) {
        processor.getProfileManager().setMacroEnergy(v);
    });
    setupMacro(macroWarmBrightSlider, macroWarmBrightLabel, "Tone", [this](float v) {
        processor.getProfileManager().setMacroWarmBright(v);
    });
    setupMacro(macroSpaceSlider, macroSpaceLabel, "Space", [this](float v) {
        processor.getProfileManager().setMacroSpace(v);
    });
    setupMacro(macroSaturationSlider, macroSaturationLabel, "Drive", [this](float v) {
        processor.getProfileManager().setMacroSaturation(v);
    });

    sessionModeBox.addItem("Live Mode", 1);
    sessionModeBox.addItem("Learn Mode", 2);
    sessionModeBox.addItem("Freeze All", 3);
    sessionModeBox.setSelectedId(1, juce::dontSendNotification);
    sessionModeBox.onChange = [this]() {
        int id = sessionModeBox.getSelectedId();
        if (id == 2) processor.getProfileManager().setSessionMode(SessionMode::Learn);
        else if (id == 3) processor.getProfileManager().setSessionMode(SessionMode::FreezeAll);
        else processor.getProfileManager().setSessionMode(SessionMode::Live);
    };
    addAndMakeVisible(sessionModeBox);

    learnedStatusLabel.setFont(juce::FontOptions(10.0f));
    learnedStatusLabel.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getTextSecondary());
    learnedStatusLabel.setText("Session: Live", juce::dontSendNotification);
    addAndMakeVisible(learnedStatusLabel);

    resetLearnedButton.onClick = [this]() {
        processor.getProfileManager().resetActiveProfileToDefault();
    };
    addAndMakeVisible(resetLearnedButton);

    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&gridContainer, false);
    viewport.setScrollBarsShown(true, false);

    startTimerHz(30); // 30 fps smooth meter & spectrum refresh
}

MasterMixView::~MasterMixView() {
    stopTimer();
}

void MasterMixView::paint(juce::Graphics& g) {
    g.fillAll(AIMixerLookAndFeel::getDarkBackground());

    auto headerArea = getLocalBounds().removeFromTop(44).reduced(16, 6);

    // Title
    g.setColour(AIMixerLookAndFeel::getTextPrimary());
    g.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    g.drawText("MASTER MIX VIEW", headerArea.removeFromLeft(180), juce::Justification::centredLeft);

    // Count
    size_t count = instanceNode.getRegistry().getActiveInstanceCount();
    g.setColour(AIMixerLookAndFeel::getTextSecondary());
    g.setFont(juce::FontOptions(12.0f));
    juce::String countText = juce::String(count) + " Instances Connected in Shared Registry";
    g.drawText(countText, headerArea, juce::Justification::centredRight);
}

void MasterMixView::resized() {
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop(40).reduced(16, 4);
    header.removeFromLeft(190);
    globalSolverToggle.setBounds(header.removeFromLeft(160));

    // MVP 5 Taste Controls Bar (40px)
    auto tasteBar = bounds.removeFromTop(40).reduced(16, 4);
    genreBox.setBounds(tasteBar.removeFromLeft(130).reduced(0, 2));
    tasteBar.removeFromLeft(6);

    followLabel.setBounds(tasteBar.removeFromLeft(35));
    followSlider.setBounds(tasteBar.removeFromLeft(85));
    tasteBar.removeFromLeft(8);

    auto placeMacro = [&tasteBar](juce::Label& l, juce::Slider& s) {
        l.setBounds(tasteBar.removeFromLeft(30));
        s.setBounds(tasteBar.removeFromLeft(50));
        tasteBar.removeFromLeft(4);
    };

    placeMacro(macroVocalForwardLabel, macroVocalForwardSlider);
    placeMacro(macroEnergyLabel, macroEnergySlider);
    placeMacro(macroWarmBrightLabel, macroWarmBrightSlider);
    placeMacro(macroSpaceLabel, macroSpaceSlider);
    placeMacro(macroSaturationLabel, macroSaturationSlider);

    tasteBar.removeFromLeft(6);
    sessionModeBox.setBounds(tasteBar.removeFromLeft(95).reduced(0, 2));
    tasteBar.removeFromLeft(6);
    resetLearnedButton.setBounds(tasteBar.removeFromLeft(85).reduced(0, 2));
    tasteBar.removeFromLeft(8);
    learnedStatusLabel.setBounds(tasteBar);

    bounds.removeFromTop(4); // Gap
    viewport.setBounds(bounds);
    refreshInstanceTiles();
}

void MasterMixView::refreshInstanceTiles() {
    auto snapshot = instanceNode.getSessionSnapshot();
    int activeCount = static_cast<int>(snapshot.size());

    // Adjust tile count if instances joined or left
    if (activeCount != static_cast<int>(tiles.size())) {
        tiles.clear();
        gridContainer.removeAllChildren();

        for (int i = 0; i < activeCount; ++i) {
            auto tile = std::make_unique<InstanceTileComponent>();
            gridContainer.addAndMakeVisible(tile.get());
            tiles.push_back(std::move(tile));
        }
    }

    // Update data for each tile
    for (size_t i = 0; i < snapshot.size() && i < tiles.size(); ++i) {
        tiles[i]->updateData(snapshot[i]);
    }

    // Grid Layout: 3 or 4 columns based on width
    int viewWidth = viewport.getWidth() - 16;
    if (viewWidth < 300) viewWidth = 600;

    int cols = std::max(1, viewWidth / 260);
    int colWidth = (viewWidth - (cols - 1) * 10) / cols;
    const int tileHeight = 140;

    int rows = (activeCount + cols - 1) / std::max(1, cols);
    int totalHeight = std::max(viewport.getHeight(), rows * (tileHeight + 10) + 20);

    gridContainer.setSize(viewWidth, totalHeight);

    for (int i = 0; i < activeCount && i < static_cast<int>(tiles.size()); ++i) {
        int r = i / cols;
        int c = i % cols;
        int x = c * (colWidth + 10);
        int y = r * (tileHeight + 10) + 10;
        tiles[i]->setBounds(x, y, colWidth, tileHeight);
    }
}

void MasterMixView::timerCallback() {
    ++timerTickCount;
    auto snapshot = instanceNode.getSessionSnapshot();
    if (timerTickCount == 1 || timerTickCount == 2 || timerTickCount == 5 || timerTickCount == 20 || (timerTickCount % 60 == 0)) {
        traceLog("MasterMixView timerCallback (tick " + juce::String(timerTickCount) +
                 "): tiles=" + juce::String(tiles.size()) +
                 ", snapshotEntries=" + juce::String(snapshot.size()));
    }

    if (globalSolverToggle.getToggleState() != processor.isGlobalSolverEnabled()) {
        bool on = processor.isGlobalSolverEnabled();
        globalSolverToggle.setToggleState(on, juce::dontSendNotification);
        globalSolverToggle.setButtonText(on ? "Global Solver: ON" : "Global Solver: OFF");
    }

    // Update Learned Status
    auto msg = processor.getProfileManager().getLastLearnedMessage();
    learnedStatusLabel.setText(msg, juce::dontSendNotification);

    refreshInstanceTiles();
    repaint(0, 0, getWidth(), 85); // Repaint header and taste bar
}

} // namespace aimixer
