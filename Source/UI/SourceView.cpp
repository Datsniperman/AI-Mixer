#include "SourceView.h"
#include "LookAndFeel.h"
#include "../PluginProcessor.h"
#include "../Common/LoadTrace.h"

namespace aimixer {

SourceView::SourceView(AIMixerAudioProcessor& proc)
    : processor(proc), wavesRack(proc.getHostedChain()) {
    wavesRack.setProcessor(&proc);
    addAndMakeVisible(wavesRack);
    auto& apvts = processor.getAPVTS();

    // Instrument Dropdown
    juce::StringArray instruments {
        "Auto (Parsed/Classified)",
        "Lead Vocal", "Pastor / Speech", "BGV", "Choir",
        "Kick", "Snare", "Toms", "Hi-Hat", "Overheads",
        "Bass", "Acoustic Gtr", "Electric Gtr",
        "Piano", "Keys / Organ", "Pad / Ambient", "Synth Lead/Arp",
        "Tracks / Playback", "Click (Pass-Through)", "Room / Crowd", "Generic / Unknown"
    };

    for (int i = 0; i < instruments.size(); ++i) {
        instrumentBox.addItem(instruments[i], i + 1);
    }
    addAndMakeVisible(instrumentBox);
    instrumentAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "instrument", instrumentBox
    );

    // Identity Source & Confidence label
    identitySourceLabel.setText("Identity: Auto", juce::dontSendNotification);
    identitySourceLabel.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getAccentBlue());
    identitySourceLabel.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    addAndMakeVisible(identitySourceLabel);

    // Auto-Trim Toggle & Manual Trim Slider
    addAndMakeVisible(autoTrimToggle);
    autoTrimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "autoTrim", autoTrimToggle
    );

    manualTrimSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    manualTrimSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 20);
    manualTrimSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(manualTrimSlider);
    manualTrimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "manualTrim", manualTrimSlider
    );

    // HPF Controls
    hpfSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    hpfSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 20);
    hpfSlider.setTextValueSuffix(" Hz");
    hpfSlider.onValueChange = [this]() {
        if (hpfSlider.isMouseButtonDown()) {
            processor.getSafeChain().setHpfFrequency(static_cast<float>(hpfSlider.getValue()), true);
        }
    };
    addAndMakeVisible(hpfSlider);
    hpfAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "hpfFreq", hpfSlider
    );

    hpfResetButton.onClick = [this]() {
        processor.getSafeChain().resetHpfToAuto();
        hpfSlider.setValue(processor.getSafeChain().getHpfHz(), juce::dontSendNotification);
    };
    addAndMakeVisible(hpfResetButton);

    // LPF Controls
    lpfSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    lpfSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 20);
    lpfSlider.setTextValueSuffix(" Hz");
    lpfSlider.onValueChange = [this]() {
        if (lpfSlider.isMouseButtonDown()) {
            processor.getSafeChain().setLpfFrequency(static_cast<float>(lpfSlider.getValue()), true);
        }
    };
    addAndMakeVisible(lpfSlider);
    lpfAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "lpfFreq", lpfSlider
    );

    lpfResetButton.onClick = [this]() {
        processor.getSafeChain().resetLpfToAuto();
        lpfSlider.setValue(processor.getSafeChain().getLpfHz(), juce::dontSendNotification);
    };
    addAndMakeVisible(lpfResetButton);

    // Leveler Compressor Controls
    addAndMakeVisible(compToggle);
    compAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "compEnabled", compToggle
    );

    compThreshSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    compThreshSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 20);
    compThreshSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(compThreshSlider);
    compThreshAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "compThresh", compThreshSlider
    );

    // Status Label
    statusLabel.setText("Status: Safe Chain Active", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getTextSecondary());
    statusLabel.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(statusLabel);

    // Coordinator Badge
    coordinatorBadge.setText("* ELECTED COORDINATOR", juce::dontSendNotification);
    coordinatorBadge.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getAccentOrange());
    coordinatorBadge.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    coordinatorBadge.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(coordinatorBadge);

    // Solver Section Controls
    solverToggle.setToggleState(true, juce::dontSendNotification);
    solverToggle.onClick = [this]() {
        processor.getSolverStage().setSolverEnabled(solverToggle.getToggleState());
    };
    addAndMakeVisible(solverToggle);

    solverReadoutLabel.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    solverReadoutLabel.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getAccentBlue());
    addAndMakeVisible(solverReadoutLabel);

    auto setupReason = [this](juce::Label& lbl) {
        lbl.setFont(juce::FontOptions(10.0f));
        lbl.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getTextSecondary());
        addAndMakeVisible(lbl);
    };
    setupReason(solverReasonLabel1);
    setupReason(solverReasonLabel2);
    setupReason(solverReasonLabel3);

    // MVP 5 Controls
    moreLessSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    moreLessSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 18);
    moreLessSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(moreLessSlider);
    moreLessAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "moreLess", moreLessSlider
    );
    moreLessLabel.setFont(juce::FontOptions(11.0f));
    moreLessLabel.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getTextSecondary());
    addAndMakeVisible(moreLessLabel);

    addAndMakeVisible(localTailToggle);
    localTailAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "localTail", localTailToggle
    );

    sendTrimSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    sendTrimSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 18);
    sendTrimSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(sendTrimSlider);
    sendTrimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "sendTrim", sendTrimSlider
    );
    sendTrimLabel.setFont(juce::FontOptions(11.0f));
    sendTrimLabel.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getTextSecondary());
    addAndMakeVisible(sendTrimLabel);

    sessionModePill.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    sessionModePill.setJustificationType(juce::Justification::centred);
    sessionModePill.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getAccentGreen());
    addAndMakeVisible(sessionModePill);

    startTimerHz(30);
}

SourceView::~SourceView() {
    stopTimer();
}

void SourceView::paint(juce::Graphics& g) {
    g.fillAll(AIMixerLookAndFeel::getDarkBackground());

    auto bounds = getLocalBounds().reduced(16);

    // Top Section
    auto topArea = bounds.removeFromTop(36);
    g.setColour(AIMixerLookAndFeel::getTextPrimary());
    g.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
    g.drawText("SOURCE STRIP", topArea.removeFromLeft(120), juce::Justification::centredLeft);

    // Main layout: Center Spectrum + Right Meters
    bounds.removeFromTop(12);
    auto bottomBar = bounds.removeFromBottom(28);
    auto solverArea = bounds.removeFromBottom(74);
    bounds.removeFromBottom(8); // Gap
    auto wavesRackArea = bounds.removeFromBottom(120);
    wavesRack.setBounds(wavesRackArea);
    bounds.removeFromBottom(8);

    auto safeChainArea = bounds.removeFromBottom(106);
    bounds.removeFromBottom(8); // Gap

    auto meterArea = bounds.removeFromRight(46);
    bounds.removeFromRight(12); // Gap
    auto spectrumArea = bounds;

    drawSpectrum(g, spectrumArea.toFloat());
    drawMeters(g, meterArea.toFloat());
    drawSafeChainStrip(g, safeChainArea.toFloat());

    // Draw Solver Card background
    g.setColour(AIMixerLookAndFeel::getCardBackground());
    g.fillRoundedRectangle(solverArea.toFloat(), 6.0f);
    g.setColour(AIMixerLookAndFeel::getBorderColour());
    g.drawRoundedRectangle(solverArea.toFloat(), 6.0f, 1.0f);

    auto solHeader = solverArea.removeFromTop(24).reduced(8, 4);
    g.setColour(AIMixerLookAndFeel::getTextPrimary());
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawText("GLOBAL MIX SOLVER DIRECTIVES", solHeader, juce::Justification::centredLeft);
}

void SourceView::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> area) {
    g.setColour(AIMixerLookAndFeel::getCardBackground());
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(AIMixerLookAndFeel::getBorderColour());
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    auto plotArea = area.reduced(10.0f);

    // Frequency grid lines
    g.setColour(AIMixerLookAndFeel::getBorderColour().withAlpha(0.25f));
    for (float db : { -12.0f, -24.0f, -36.0f, -48.0f }) {
        float y = plotArea.getY() + ((-db) / 60.0f) * plotArea.getHeight();
        g.drawHorizontalLine(static_cast<int>(y), plotArea.getX(), plotArea.getRight());
    }

    const float barSpacing = 2.0f;
    const float totalSpacing = barSpacing * (NUM_SPECTRUM_BANDS - 1);
    const float barWidth = (plotArea.getWidth() - totalSpacing) / static_cast<float>(NUM_SPECTRUM_BANDS);

    juce::ColourGradient grad(
        AIMixerLookAndFeel::getAccentBlue(), plotArea.getX(), plotArea.getBottom(),
        AIMixerLookAndFeel::getAccentGreen(), plotArea.getX(), plotArea.getY(), false
    );
    g.setGradientFill(grad);

    for (size_t b = 0; b < NUM_SPECTRUM_BANDS; ++b) {
        float normalized = std::clamp(currentSpectrum[b], 0.0f, 1.0f);
        float h = normalized * plotArea.getHeight();
        float x = plotArea.getX() + b * (barWidth + barSpacing);
        float y = plotArea.getBottom() - h;

        g.fillRect(x, y, barWidth, std::max(2.0f, h));
    }
}

void SourceView::drawMeters(juce::Graphics& g, juce::Rectangle<float> area) {
    g.setColour(AIMixerLookAndFeel::getCardBackground());
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(AIMixerLookAndFeel::getBorderColour());
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    auto meterBar = area.reduced(6.0f, 10.0f);

    float normRms = std::clamp((currentRmsDb + 60.0f) / 60.0f, 0.0f, 1.0f);
    float normPeak = std::clamp((currentPeakDb + 60.0f) / 60.0f, 0.0f, 1.0f);

    float rmsHeight = normRms * meterBar.getHeight();
    juce::Colour rmsColor = (currentRmsDb > -6.0f) ? AIMixerLookAndFeel::getAccentOrange() :
                            (currentRmsDb > -18.0f) ? AIMixerLookAndFeel::getAccentGreen() :
                                                      AIMixerLookAndFeel::getAccentBlue();
    g.setColour(rmsColor);
    g.fillRect(meterBar.getX(), meterBar.getBottom() - rmsHeight, meterBar.getWidth() * 0.55f, rmsHeight);

    float peakY = meterBar.getBottom() - (normPeak * meterBar.getHeight());
    g.setColour(juce::Colours::white);
    g.fillRect(meterBar.getX(), peakY, meterBar.getWidth(), 2.0f);
}

void SourceView::drawSafeChainStrip(juce::Graphics& g, juce::Rectangle<float> area) {
    g.setColour(AIMixerLookAndFeel::getCardBackground());
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(AIMixerLookAndFeel::getBorderColour());
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    auto header = area.removeFromTop(24).reduced(8, 4);
    g.setColour(AIMixerLookAndFeel::getTextPrimary());
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawText("UNIVERSAL SAFE CHAIN", header, juce::Justification::centredLeft);

    // Draw gain reduction meter for compressor
    float gr = processor.getSafeChain().getCompGainReductionDb();
    if (gr > 0.1f) {
        auto grArea = header.removeFromRight(120);
        g.setColour(AIMixerLookAndFeel::getAccentOrange());
        g.drawText("GR: -" + juce::String(gr, 1) + " dB", grArea, juce::Justification::centredRight);
    }
}

void SourceView::resized() {
    auto bounds = getLocalBounds().reduced(16);
    auto topArea = bounds.removeFromTop(36);
    topArea.removeFromLeft(130);

    instrumentBox.setBounds(topArea.removeFromLeft(190).reduced(0, 4));
    topArea.removeFromLeft(8);
    identitySourceLabel.setBounds(topArea.removeFromLeft(180));
    coordinatorBadge.setBounds(topArea);

    auto bottomArea = bounds.removeFromBottom(24);
    statusLabel.setBounds(bottomArea);

    // MVP 5 Taste / FX Toolbar (26px)
    auto mvp5Row = bounds.removeFromBottom(26);
    bounds.removeFromBottom(6);

    moreLessLabel.setBounds(mvp5Row.removeFromLeft(75));
    moreLessSlider.setBounds(mvp5Row.removeFromLeft(125));
    mvp5Row.removeFromLeft(8);

    localTailToggle.setBounds(mvp5Row.removeFromLeft(85));
    mvp5Row.removeFromLeft(8);

    sendTrimLabel.setBounds(mvp5Row.removeFromLeft(65));
    sendTrimSlider.setBounds(mvp5Row.removeFromLeft(120));
    mvp5Row.removeFromLeft(8);

    sessionModePill.setBounds(mvp5Row);

    // Solver Area (74px)
    auto solverArea = bounds.removeFromBottom(74);
    bounds.removeFromBottom(8);

    auto solHeader = solverArea.removeFromTop(24).reduced(8, 2);
    solverToggle.setBounds(solHeader.removeFromRight(95));
    solverReadoutLabel.setBounds(solHeader.removeFromRight(150));

    auto reasonInner = solverArea.reduced(8, 2);
    int reasonW = reasonInner.getWidth() / 3;
    solverReasonLabel1.setBounds(reasonInner.removeFromLeft(reasonW).reduced(2, 0));
    solverReasonLabel2.setBounds(reasonInner.removeFromLeft(reasonW).reduced(2, 0));
    solverReasonLabel3.setBounds(reasonInner.reduced(2, 0));

    // Safe Chain Area (106px)
    auto wavesRackArea = bounds.removeFromBottom(120);
    wavesRack.setBounds(wavesRackArea);
    bounds.removeFromBottom(8);

    auto safeChainArea = bounds.removeFromBottom(106);
    safeChainArea.removeFromTop(24);
    auto inner = safeChainArea.reduced(8, 2);

    int rowH = inner.getHeight() / 3;

    // Row 1: Auto-Trim + Manual Trim
    auto row1 = inner.removeFromTop(rowH).reduced(0, 2);
    autoTrimToggle.setBounds(row1.removeFromLeft(100));
    row1.removeFromLeft(8);
    manualTrimSlider.setBounds(row1);

    // Row 2: HPF + LPF
    auto row2 = inner.removeFromTop(rowH).reduced(0, 2);
    int colW = row2.getWidth() / 2;
    auto hpfCol = row2.removeFromLeft(colW).reduced(2, 0);
    hpfResetButton.setBounds(hpfCol.removeFromRight(46).reduced(0, 2));
    hpfSlider.setBounds(hpfCol);

    auto lpfCol = row2.reduced(2, 0);
    lpfResetButton.setBounds(lpfCol.removeFromRight(46).reduced(0, 2));
    lpfSlider.setBounds(lpfCol);

    // Row 3: Leveller Compressor
    auto row3 = inner.reduced(0, 2);
    compToggle.setBounds(row3.removeFromLeft(100));
    row3.removeFromLeft(8);
    compThreshSlider.setBounds(row3);
}

void SourceView::timerCallback() {
    ++timerTickCount;
    if (timerTickCount == 1 || timerTickCount == 2 || timerTickCount == 5 || timerTickCount == 20 || (timerTickCount % 60 == 0)) {
        traceLog("SourceView timerCallback (tick " + juce::String(timerTickCount) + ")");
    }

    currentRmsDb = processor.getAnalyzer().getInstantRmsDb();
    currentPeakDb = processor.getAnalyzer().getInstantPeakDb();

    InstanceTelemetry telem;
    processor.getAnalyzer().updateAnalysis(telem);
    for (size_t b = 0; b < NUM_SPECTRUM_BANDS; ++b) {
        currentSpectrum[b] = telem.spectrumBands[b];
    }

    coordinatorBadge.setVisible(processor.getInstanceNode().isCoordinator());

    // Update Identity badge & label
    auto source = processor.getIdentitySource();
    auto inst = processor.getResolvedInstrument();
    float conf = processor.getResolvedConfidence();

    juce::String srcText = getInstrumentName(inst);
    srcText += " (" + juce::String(static_cast<int>(conf * 100.0f)) + "% - ";
    srcText += getIdentitySourceName(source);
    srcText += ")";
    identitySourceLabel.setText(srcText, juce::dontSendNotification);

    // Update Status line
    auto& sc = processor.getSafeChain();
    juce::String status = "Auto-trim " + juce::String(sc.getAutoTrimDb(), 1) + " dB -> -18 dBFS | HPF "
                        + juce::String(static_cast<int>(sc.getHpfHz())) + " Hz (" + getInstrumentName(inst) + ") | "
                        + "Comp " + juce::String(sc.getCompGainReductionDb(), 1) + " dB GR";

    if (sc.isAutoTrimAtLimit()) {
        status += " (at trim limit)";
    }

    if (inst == InstrumentType::Click) {
        status = "Click Track: Safe Chain Bypassed (Pass-Through)";
    }
    statusLabel.setText(status, juce::dontSendNotification);

    // Update Solver Section Readouts & Top 3 Reasons
    auto& solver = processor.getSolverStage();
    float ride = solver.getActiveRideDb();
    float duck = solver.getActiveDuckDb();
    juce::String solText = juce::String("Ride: ") + (ride >= 0 ? "+" : "") + juce::String(ride, 1) + " dB";
    if (duck < -0.1f) {
        solText += " | Duck: " + juce::String(duck, 1) + " dB";
    }
    solverReadoutLabel.setText(solText, juce::dontSendNotification);

    juce::Label* labels[3] = { &solverReasonLabel1, &solverReasonLabel2, &solverReasonLabel3 };
    int labelIdx = 0;

    for (int k = 0; k < 4 && labelIdx < 3; ++k) {
        auto cut = solver.getActiveCut(k);
        if (cut.enabled) {
            juce::String rStr = juce::String(cut.gainDb, 1) + " dB @" + juce::String(static_cast<int>(cut.freqHz))
                              + " Hz (" + getReasonDescription(cut.reason) + ")";
            labels[labelIdx]->setText(rStr, juce::dontSendNotification);
            labelIdx++;
        }
    }
    while (labelIdx < 3) {
        labels[labelIdx]->setText(labelIdx == 0 && !solver.isSolverActive() ? "No solver cuts active" : "", juce::dontSendNotification);
        labelIdx++;
    }

    // Update Session Mode Pill
    auto smode = processor.getCurrentTasteState().sessionMode;
    if (smode == SessionMode::Learn) {
        sessionModePill.setText("LEARN MODE", juce::dontSendNotification);
        sessionModePill.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getAccentOrange());
    } else if (smode == SessionMode::FreezeAll) {
        sessionModePill.setText("FROZEN", juce::dontSendNotification);
        sessionModePill.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getAccentBlue());
    } else {
        sessionModePill.setText("LIVE", juce::dontSendNotification);
        sessionModePill.setColour(juce::Label::textColourId, AIMixerLookAndFeel::getAccentGreen());
    }

    repaint();
}

} // namespace aimixer



