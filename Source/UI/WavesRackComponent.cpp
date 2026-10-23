#include "WavesRackComponent.h"
#include "LookAndFeel.h"
#include "../PluginProcessor.h"

namespace aimixer {

WavesRackComponent::WavesRackComponent(HostedPluginChain& c)
    : chain(c) {
    masterBypassToggle.setToggleState(!chain.isMasterBypassed(), juce::dontSendNotification);
    masterBypassToggle.onClick = [this]() {
        chain.setMasterBypass(!masterBypassToggle.getToggleState());
    };
        addAndMakeVisible(masterBypassToggle);

    autoCurateButton.onClick = [this]() {
        if (processor != nullptr) {
            processor->curateAndLoadChain();
        }
    };
    addAndMakeVisible(autoCurateButton);

    latencyLabel.setFont(juce::FontOptions(11.0f));
    latencyLabel.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getTextSecondary());
    latencyLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(latencyLabel);

    auto allPlugins = PluginCurator::getInstance().getAllWavesPlugins();

    for (size_t i = 0; i < chain.getNumSlots(); ++i) {
        auto ui = std::make_unique<SlotUI>();
        size_t slotIdx = i;

        ui->roleLabel.setText("Slot " + juce::String(i + 1), juce::dontSendNotification);
        ui->roleLabel.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
        ui->roleLabel.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getAccentBlue());
        addAndMakeVisible(ui->roleLabel);

        // Populate combo box with available Waves plugins
        ui->pluginSelector.addItem("(Empty / Disabled)", 1);
        int itemIndex = 2;
        for (const auto& p : allPlugins) {
            ui->pluginSelector.addItem(p.name, itemIndex++);
        }
        ui->pluginSelector.setSelectedId(1, juce::dontSendNotification);
        ui->pluginSelector.onChange = [this, slotIdx, allPlugins]() {
            int sel = slotUIs[slotIdx]->pluginSelector.getSelectedId();
            if (sel <= 1) {
                chain.clearSlot(slotIdx);
            } else {
                size_t pIndex = static_cast<size_t>(sel - 2);
                if (pIndex < allPlugins.size()) {
                    chain.loadSlot(slotIdx, allPlugins[pIndex]);
                }
            }
        };
        addAndMakeVisible(ui->pluginSelector);

        ui->openGuiButton.onClick = [this, slotIdx]() {
            if (auto* s = chain.getSlot(slotIdx)) {
                if (s->isLoaded()) {
                    if (s->isEditorOpen()) {
                        s->closeEditor();
                    } else {
                        s->showEditor();
                    }
                }
            }
        };
        addAndMakeVisible(ui->openGuiButton);

        ui->bypassToggle.onClick = [this, slotIdx]() {
            if (auto* s = chain.getSlot(slotIdx)) {
                s->setBypassed(slotUIs[slotIdx]->bypassToggle.getToggleState());
            }
        };
        addAndMakeVisible(ui->bypassToggle);

        ui->clearButton.onClick = [this, slotIdx]() {
            chain.clearSlot(slotIdx);
            slotUIs[slotIdx]->pluginSelector.setSelectedId(1, juce::dontSendNotification);
        };
        addAndMakeVisible(ui->clearButton);

        slotUIs.push_back(std::move(ui));
    }

    startTimerHz(10);
}

WavesRackComponent::~WavesRackComponent() {
    stopTimer();
}

void WavesRackComponent::timerCallback() {
    updateDisplay();
}

void WavesRackComponent::updateDisplay() {
    int lat = chain.getTotalLatencySamples();
    if (lat == 0) {
        latencyLabel.setText("Latency: 0 samples (Live Ready)", juce::dontSendNotification);
        latencyLabel.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getAccentGreen());
    } else {
        float ms = (lat * 1000.0f) / 48000.0f;
        latencyLabel.setText("Latency: " + juce::String(lat) + " spls (" + juce::String(ms, 1) + " ms)", juce::dontSendNotification);
        latencyLabel.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getAccentOrange());
    }

    auto allPlugins = PluginCurator::getInstance().getAllWavesPlugins();

    for (size_t i = 0; i < slotUIs.size(); ++i) {
        auto* slot = chain.getSlot(i);
        auto& ui = *slotUIs[i];

        if (slot && slot->isLoaded()) {
            ui.openGuiButton.setEnabled(true);
            ui.clearButton.setEnabled(true);
            ui.bypassToggle.setEnabled(true);
            ui.bypassToggle.setToggleState(slot->isBypassed(), juce::dontSendNotification);

            juce::String curName = slot->getPluginName();
            for (int item = 2; item <= ui.pluginSelector.getNumItems(); ++item) {
                if (ui.pluginSelector.getItemText(item - 1) == curName) {
                    if (ui.pluginSelector.getSelectedId() != item) {
                        ui.pluginSelector.setSelectedId(item, juce::dontSendNotification);
                    }
                    break;
                }
            }
        } else {
            ui.openGuiButton.setEnabled(false);
            ui.clearButton.setEnabled(false);
            ui.bypassToggle.setEnabled(false);
            if (ui.pluginSelector.getSelectedId() != 1) {
                ui.pluginSelector.setSelectedId(1, juce::dontSendNotification);
            }
        }
    }
}

void WavesRackComponent::paint(juce::Graphics& g) {
    g.setColour(AIMixerLookAndFeel::getCardBackground());
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
    g.setColour(AIMixerLookAndFeel::getBorderColour());
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 6.0f, 1.0f);

    auto header = getLocalBounds().removeFromTop(24).reduced(8, 4);
    g.setColour(AIMixerLookAndFeel::getTextPrimary());
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawText("AUTONOMOUS WAVES INSERT RACK", header, juce::Justification::centredLeft);
}

void WavesRackComponent::resized() {
    auto bounds = getLocalBounds().reduced(8);
    auto topRow = bounds.removeFromTop(22);
        masterBypassToggle.setBounds(topRow.removeFromLeft(110));
    topRow.removeFromLeft(8);
    autoCurateButton.setBounds(topRow.removeFromLeft(140));
    latencyLabel.setBounds(topRow.removeFromRight(200));

    bounds.removeFromTop(6);

    int numSlots = static_cast<int>(slotUIs.size());
    if (numSlots == 0) return;

    int slotHeight = (bounds.getHeight() - (numSlots - 1) * 4) / numSlots;

    for (size_t i = 0; i < slotUIs.size(); ++i) {
        auto row = bounds.removeFromTop(slotHeight);
        bounds.removeFromTop(4);

        auto& ui = *slotUIs[i];
        ui.roleLabel.setBounds(row.removeFromLeft(48).reduced(0, 2));
        ui.clearButton.setBounds(row.removeFromRight(22).reduced(1, 2));
        ui.openGuiButton.setBounds(row.removeFromRight(44).reduced(2, 2));
        ui.bypassToggle.setBounds(row.removeFromRight(64).reduced(2, 2));
        row.removeFromRight(4);
        ui.pluginSelector.setBounds(row.reduced(0, 2));
    }
}

} // namespace aimixer

