#include "PluginEditor.h"
#include "Common/LoadTrace.h"

namespace aimixer {

AIMixerAudioProcessorEditor::AIMixerAudioProcessorEditor(AIMixerAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
    traceLog("editor ctor start");
    setLookAndFeel(&customLookAndFeel);

    // Title
    titleLabel.setText("AI MIX ENGINE v1.0.0", juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getTextPrimary());
    titleLabel.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    addAndMakeVisible(titleLabel);

    // Role Switcher Buttons
    auto setupRoleButton = [this](juce::TextButton& btn, PluginRole role) {
        btn.onClick = [this, role]() {
            audioProcessor.setRole(role);
            updateActiveView();
        };
        addAndMakeVisible(btn);
    };

    setupRoleButton(sourceRoleButton, PluginRole::Source);
    setupRoleButton(busRoleButton, PluginRole::Bus);
    setupRoleButton(masterRoleButton, PluginRole::Master);

    // Destination Box (PA / Broadcast / Studio)
    destinationBox.addItem("PA / Live", 1);
    destinationBox.addItem("Broadcast", 2);
    destinationBox.addItem("Studio", 3);
    addAndMakeVisible(destinationBox);
    destinationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(), "destination", destinationBox
    );

    // Coordinator Badge
    coordinatorStatusBadge.setText("* COORDINATOR", juce::dontSendNotification);
    coordinatorStatusBadge.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getAccentOrange());
    coordinatorStatusBadge.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    coordinatorStatusBadge.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(coordinatorStatusBadge);

    // Instantiate View Panes
    sourceView = std::make_unique<SourceView>(audioProcessor);
    busView = std::make_unique<BusView>(audioProcessor.getAPVTS(), audioProcessor.getInstanceNode(), audioProcessor.getAnalyzer());
    masterMixView = std::make_unique<MasterMixView>(audioProcessor);

    addChildComponent(sourceView.get());
    addChildComponent(busView.get());
    addChildComponent(masterMixView.get());

    updateActiveView();

    setResizable(true, true);
    setResizeLimits(700, 480, 1600, 1000);
    setSize(920, 580);

    startTimerHz(10);
    auto snapshot = audioProcessor.getInstanceNode().getSessionSnapshot();
    size_t tileCount = masterMixView ? masterMixView->getTileCount() : 0;
    traceLog("editor ctor end: snapshotCount=" + juce::String(snapshot.size()) +
             ", tileVectorSize=" + juce::String(tileCount) +
             ", spectrumArraySize=" + juce::String(NUM_SPECTRUM_BANDS));
}

AIMixerAudioProcessorEditor::~AIMixerAudioProcessorEditor() {
    traceLog("editor dtor start");
    stopTimer();
    setLookAndFeel(nullptr);
    traceLog("editor dtor end");
}

void AIMixerAudioProcessorEditor::visibilityChanged() {
    traceLog("editor visibilityChanged: visible=" + juce::String(isVisible() ? 1 : 0) +
             ", showing=" + juce::String(isShowing() ? 1 : 0) +
             ", onDesktop=" + juce::String(isOnDesktop() ? 1 : 0));
}

void AIMixerAudioProcessorEditor::parentHierarchyChanged() {
    traceLog("editor parentHierarchyChanged: onDesktop=" + juce::String(isOnDesktop() ? 1 : 0) +
             ", hasParent=" + juce::String(getParentComponent() != nullptr ? 1 : 0));
}

void AIMixerAudioProcessorEditor::updateActiveView() {
    PluginRole role = audioProcessor.getCurrentRole();
    activeViewRole = role;

    sourceRoleButton.setToggleState(role == PluginRole::Source, juce::dontSendNotification);
    busRoleButton.setToggleState(role == PluginRole::Bus, juce::dontSendNotification);
    masterRoleButton.setToggleState(role == PluginRole::Master, juce::dontSendNotification);

    sourceView->setVisible(role == PluginRole::Source);
    busView->setVisible(role == PluginRole::Bus);
    masterMixView->setVisible(role == PluginRole::Master);

    destinationBox.setVisible(role == PluginRole::Master);

    resized();
}

void AIMixerAudioProcessorEditor::paint(juce::Graphics& g) {
    if (paintCount < 3) {
        traceLog("editor paint start (count " + juce::String(paintCount + 1) + ")");
    }

    g.fillAll(AIMixerLookAndFeel::getDarkBackground());

    // Top Header Background Bar
    auto headerBounds = getLocalBounds().removeFromTop(52);
    g.setColour(AIMixerLookAndFeel::getPanelBackground());
    g.fillRect(headerBounds);

    g.setColour(AIMixerLookAndFeel::getBorderColour());
    g.drawHorizontalLine(52, 0.0f, static_cast<float>(getWidth()));

    if (paintCount < 3) {
        ++paintCount;
        traceLog("editor paint end (count " + juce::String(paintCount) + ")");
    }
}

void AIMixerAudioProcessorEditor::resized() {
    if (!hasLoggedFirstResized) {
        hasLoggedFirstResized = true;
        traceLog("editor resized (first): width=" + juce::String(getWidth()) +
                 ", height=" + juce::String(getHeight()));
    }

    auto bounds = getLocalBounds();
    auto headerArea = bounds.removeFromTop(52).reduced(12, 10);

    // Left: Title
    titleLabel.setBounds(headerArea.removeFromLeft(140));

    headerArea.removeFromLeft(20);

    // Center: Segmented Role Switcher
    auto roleArea = headerArea.removeFromLeft(230);
    int btnWidth = roleArea.getWidth() / 3;
    sourceRoleButton.setBounds(roleArea.removeFromLeft(btnWidth).reduced(1, 0));
    busRoleButton.setBounds(roleArea.removeFromLeft(btnWidth).reduced(1, 0));
    masterRoleButton.setBounds(roleArea.reduced(1, 0));

    headerArea.removeFromLeft(20);

    // Coordinator Badge
    coordinatorStatusBadge.setVisible(audioProcessor.getInstanceNode().isCoordinator());
    coordinatorStatusBadge.setBounds(headerArea.removeFromLeft(120));

    // Right: Destination Selector (only visible in Master mode)
    if (destinationBox.isVisible()) {
        destinationBox.setBounds(headerArea.removeFromRight(130));
    }

    // Body content
    if (sourceView && sourceView->isVisible()) {
        sourceView->setBounds(bounds);
    }
    if (busView && busView->isVisible()) {
        busView->setBounds(bounds);
    }
    if (masterMixView && masterMixView->isVisible()) {
        masterMixView->setBounds(bounds);
    }
}

void AIMixerAudioProcessorEditor::timerCallback() {
    PluginRole role = audioProcessor.getCurrentRole();
    if (role != activeViewRole) {
        updateActiveView();
    }
    bool isCoord = audioProcessor.getInstanceNode().isCoordinator();
    if (coordinatorStatusBadge.isVisible() != isCoord) {
        coordinatorStatusBadge.setVisible(isCoord);
    }
}

} // namespace aimixer
