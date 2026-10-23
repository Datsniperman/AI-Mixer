#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace aimixer {

juce::AudioProcessorValueTreeState::ParameterLayout AIMixerAudioProcessor::createParameterLayout() {
    traceLog("processor ctor: createParameterLayout start");
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Role selection
    juce::StringArray roles { "Source", "Bus", "Master" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "role", 1 }, "Role", roles, 0
    ));

    // Master destination tag
    juce::StringArray destinations { "PA / Live", "Broadcast", "Studio" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "destination", 1 }, "Destination", destinations, 0
    ));

    // Instrument selection (Auto + 19 categories + Generic/Unknown)
    juce::StringArray instruments {
        "Auto (Parsed/Classified)",
        "Lead Vocal", "Pastor / Speech", "BGV", "Choir",
        "Kick", "Snare", "Toms", "Hi-Hat", "Overheads",
        "Bass", "Acoustic Gtr", "Electric Gtr",
        "Piano", "Keys / Organ", "Pad / Ambient", "Synth Lead/Arp",
        "Tracks / Playback", "Click (Pass-Through)", "Room / Crowd", "Generic / Unknown"
    };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "instrument", 1 }, "Instrument", instruments, 0
    ));

    // Safe Chain parameters
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "autoTrim", 1 }, "Auto-Trim", true
    ));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "manualTrim", 1 }, "Manual Trim",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f
    ));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "hpfFreq", 1 }, "HPF Frequency",
        juce::NormalisableRange<float>(20.0f, 1000.0f, 1.0f, 0.5f), 40.0f
    ));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "lpfFreq", 1 }, "LPF Frequency",
        juce::NormalisableRange<float>(1000.0f, 20000.0f, 10.0f, 0.5f), 20000.0f
    ));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "compEnabled", 1 }, "Comp Enabled", true
    ));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compThresh", 1 }, "Comp Threshold",
        juce::NormalisableRange<float>(-40.0f, 0.0f, 0.5f), -18.0f
    ));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "compRatio", 1 }, "Comp Ratio",
        juce::NormalisableRange<float>(1.0f, 8.0f, 0.1f), 2.0f
    ));

    // Bypass & Freeze
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "bypass", 1 }, "Bypass", false
    ));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "freeze", 1 }, "Freeze", false
    ));

    // MVP 5: More/Less of Me & Local FX
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "moreLess", 1 }, "More / Less of Me",
        juce::NormalisableRange<float>(-3.0f, 3.0f, 0.1f), 0.0f
    ));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "localTail", 1 }, "Local Tail Mode", true
    ));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "sendTrim", 1 }, "Local Send Trim",
        juce::NormalisableRange<float>(-12.0f, 6.0f, 0.5f), 0.0f
    ));

    return { params.begin(), params.end() };
}

AIMixerAudioProcessor::AIMixerAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()),
      instanceNode(PluginRole::Source, "Audio Track") {
    traceLog("processor ctor: APVTS built, body start");
    
    // Strict Zero-Latency Guarantee for Source & Bus
    setLatencySamples(0);

    auto* roleParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("role"));
    if (roleParam != nullptr) {
        currentRole = static_cast<PluginRole>(roleParam->getIndex());
        instanceNode.setRole(currentRole);
    }

    startTimerHz(15);
    traceLog("processor ctor end");
}

AIMixerAudioProcessor::~AIMixerAudioProcessor() {
    traceLog("processor dtor start");
    stopTimer();
    traceLog("processor dtor end");
}

void AIMixerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    traceLog("prepareToPlay: sampleRate=%.1f, blockSize=%d, inChannels=%d, outChannels=%d",
             sampleRate, samplesPerBlock, getTotalNumInputChannels(), getTotalNumOutputChannels());
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    spec.numChannels = static_cast<uint32_t>(getTotalNumOutputChannels());

    analyzer.prepare(sampleRate, samplesPerBlock);
    safeChain.prepare(spec);
    playbookStage.prepare(spec);
    busStage.prepare(spec);
    masterChain.prepare(spec);
    loudnessMeter.prepare(sampleRate);
    solverStage.prepare(spec);
    applyRoleLatency();
}

void AIMixerAudioProcessor::releaseResources() {
    analyzer.reset();
    safeChain.reset();
    playbookStage.reset();
    busStage.reset();
    masterChain.reset();
    loudnessMeter.reset();
    solverStage.reset();
}

bool AIMixerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    const auto& mainInput  = layouts.getMainInputChannelSet();

    // Must be either Mono or Stereo output
    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    if (!mainInput.isDisabled()) {
        if (mainInput != juce::AudioChannelSet::mono() && mainInput != juce::AudioChannelSet::stereo())
            return false;

        // Disallow stereo in -> mono out (downmix not supported)
        if (mainInput == juce::AudioChannelSet::stereo() && mainOutput == juce::AudioChannelSet::mono())
            return false;
    }

    return true;
}

void AIMixerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0) return;

    int currentBlock = ++processBlockCount;
    if (currentBlock == 1 || currentBlock == 10 || currentBlock == 100 || currentBlock == 1000) {
        traceLog("processBlock (block %d): inChannels=%d, outChannels=%d, bufferChannels=%d, numSamples=%d",
                 currentBlock, totalNumInputChannels, totalNumOutputChannels, numChannels, numSamples);
    }

    // Handle mono input into stereo output: duplicate channel 0 into channel 1
    if (totalNumInputChannels == 1 && buffer.getNumChannels() >= 2) {
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }

    // Clear any unused tail output channels
    for (int ch = std::max(totalNumInputChannels, 1); ch < buffer.getNumChannels(); ++ch) {
        if (totalNumInputChannels != 1 || ch >= 2) {
            buffer.clear(ch, 0, numSamples);
        }
    }

    // Forward host BPM for tempo-synced delay
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            if (pos->getBpm().hasValue()) {
                double bpm = *pos->getBpm();
                playbookStage.setBpm(bpm);
                busStage.setBpm(bpm);
            }
        }
    }

    // Step 1: Real-time zero-allocation audio analysis
    analyzer.processAudioBlock(buffer);

    if (currentRole == PluginRole::Source) {
        // Step 2: Universal Safe Chain DSP processing (0 latency)
        safeChain.process(buffer);

        // Step 3: Per-instrument Playbook Stage (gate, charEQ, de-esser, warmth, transient, local rider, pan/width)
        playbookStage.process(buffer);

        // Step 4: Solver Directives Stage (0 latency, masking cuts + balance ride + ducking, LAST before output)
        solverStage.process(buffer);
    } else if (currentRole == PluginRole::Bus) {
        // Bus Role: Glue compression by bus type + shelf (0 latency)
        busStage.process(buffer);
    } else if (currentRole == PluginRole::Master) {
        // Master Role: Tonal balance EQ -> Master glue -> Loudness auto-gain -> 4x lookahead limiter
        masterChain.process(buffer);

        // BS.1770-5 K-weighted loudness metering tap
        const float* l = buffer.getReadPointer(0);
        const float* r = (buffer.getNumChannels() > 1) ? buffer.getReadPointer(1) : nullptr;
        loudnessMeter.processSamples(l, r, buffer.getNumSamples());
    }
}

void AIMixerAudioProcessor::setRole(PluginRole newRole) {
    if (currentRole == newRole) return;
    currentRole = newRole;

    auto* roleParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("role"));
    if (roleParam != nullptr) {
        *roleParam = static_cast<int>(newRole);
    }

    instanceNode.setRole(newRole);
    applyRoleLatency();
}

void AIMixerAudioProcessor::applyRoleLatency() {
    if (currentRole == PluginRole::Master) {
        setLatencySamples(masterChain.getReportedLatencySamples());
    } else {
        setLatencySamples(0);
    }
    updateHostDisplay();
}

void AIMixerAudioProcessor::updateTrackProperties(const TrackProperties& properties) {
    if (properties.name.has_value() && properties.name->isNotEmpty()) {
        trackName = properties.name->toStdString();
        instanceNode.setTrackName(trackName);
        currentParseResult = TrackNameParser::parse(trackName);
    }
}

void AIMixerAudioProcessor::timerCallback() {
    // Synchronize Role
    auto* roleParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("role"));
    if (roleParam != nullptr) {
        auto paramRole = static_cast<PluginRole>(roleParam->getIndex());
        if (paramRole != currentRole) {
            currentRole = paramRole;
            instanceNode.setRole(currentRole);
            applyRoleLatency();
        }
    }

    auto* bypassParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("bypass"));
    if (bypassParam != nullptr) {
        isBypassed.store(bypassParam->get(), std::memory_order_relaxed);
        safeChain.setBypassed(bypassParam->get());
    }

    auto* freezeParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("freeze"));
    if (freezeParam != nullptr) {
        isFrozen.store(freezeParam->get(), std::memory_order_relaxed);
    }

    // Synchronize MVP 5 More/Less of Me & Local FX
    auto* moreLessParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("moreLess"));
    if (moreLessParam != nullptr) {
        moreLessDb.store(moreLessParam->get(), std::memory_order_relaxed);
    }
    auto* localTailParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("localTail"));
    if (localTailParam != nullptr) {
        localTailEnabled.store(localTailParam->get(), std::memory_order_relaxed);
    }
    auto* sendTrimParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("sendTrim"));
    if (sendTrimParam != nullptr) {
        sendTrimDb.store(sendTrimParam->get(), std::memory_order_relaxed);
    }

    // Synchronize Safe Chain APVTS parameters
    auto* autoTrimParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("autoTrim"));
    if (autoTrimParam != nullptr) {
        safeChain.setAutoTrimEnabled(autoTrimParam->get());
    }
    auto* manualTrimParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("manualTrim"));
    if (manualTrimParam != nullptr) {
        if (std::abs(manualTrimParam->get() - safeChain.getManualTrimDb()) > 0.05f) {
            safeChain.setManualTrimDb(manualTrimParam->get());
        }
    }
    auto* compEnabledParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("compEnabled"));
    if (compEnabledParam != nullptr) {
        safeChain.setCompEnabled(compEnabledParam->get());
    }
    auto* compThreshParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("compThresh"));
    if (compThreshParam != nullptr) {
        safeChain.setCompThresholdDb(compThreshParam->get());
    }
    auto* compRatioParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("compRatio"));
    if (compRatioParam != nullptr) {
        safeChain.setCompRatio(compRatioParam->get());
    }

    // Identity Resolution:
    // User pick (0 = Auto; 1..19 = Specific category; 20 = Generic / Unknown)
    InstrumentType userPick = InstrumentType::Unknown;
    auto* instParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("instrument"));
    if (instParam != nullptr) {
        int idx = instParam->getIndex();
        if (idx == 0) {
            userPick = InstrumentType::Unknown; // Auto mode
        } else if (idx <= 19) {
            userPick = static_cast<InstrumentType>(idx);
        } else {
            userPick = InstrumentType::Unknown;
        }
    }

    // Layer 3: Audio Classifier
    auto features = analyzer.getAcousticFeatures();
    auto classified = classifier.classify(features);

    // Layer 4: Session Neighbour Context
    auto withContext = InstrumentClassifier::applyNeighbourContext(
        classified, instanceNode.getSessionSnapshot(), instanceNode.getInstanceId()
    );

    // Master Precedence: User > TrackName (>=0.80) > Classifier (>=0.60) > Unknown
    resolvedInstrument = InstrumentClassifier::resolveIdentity(
        userPick, currentParseResult, withContext, resolvedSource, resolvedConfidence
    );

    // Pass resolved instrument to Safe Chain and Playbook Stage
    safeChain.setInstrumentType(resolvedInstrument);

    if (currentRole == PluginRole::Source) {
        playbookStage.setInstrumentIdentity(resolvedInstrument, currentParseResult.side);
        playbookStage.updateWorkerAnalysis(analyzer.getInstantRmsDb(), -65.0f, features.spectralCentroidHz);
        playbookStage.setFrozen(isFrozen.load(std::memory_order_relaxed));
    } else if (currentRole == PluginRole::Bus) {
        BusType bType = TrackNameParser::parseBusType(trackName);
        busStage.setBusType(bType);
    } else if (currentRole == PluginRole::Master) {
        auto* destParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("destination"));
        if (destParam != nullptr) {
            masterChain.setDestination(static_cast<DestinationTag>(destParam->getIndex()));
        }
    }

    // Update Auto-Trim on worker thread
    safeChain.updateAutoTrimWorker(analyzer.getInstantRmsDb());

    uint64_t nowMs = SharedMemoryRegistry::getSystemTimeMs();

    // If elected Coordinator, run Global Mix Solver at ~5 Hz (every cycle or every 3 cycles of 15 Hz)
    if (instanceNode.isCoordinator()) {
        currentTasteState = profileManager.buildGlobalTasteState();
        instanceNode.publishTasteState(currentTasteState);

        auto snapshot = instanceNode.getSessionSnapshot();

        // Step 5: Process profile learning proposals from snapshot
        for (const auto& target : snapshot) {
            if (target.proposalSequence > 0) {
                profileManager.processProposal(target, nowMs);
            }
        }

        solverState.globalSolverEnabled = globalSolverEnabled.load(std::memory_order_relaxed);
        auto directivesMap = MixSolver::solve(snapshot, nowMs, solverState, currentTasteState);

        for (const auto& target : snapshot) {
            auto it = directivesMap.find(target.instanceId);
            if (it != directivesMap.end()) {
                // Find target's slot in registry to write directives
                for (size_t s = 0; s < MAX_REGISTRY_INSTANCES; ++s) {
                    InstanceTelemetry t;
                    if (instanceNode.getRegistry().readTelemetry(static_cast<int>(s), t) && t.instanceId == target.instanceId) {
                        instanceNode.getRegistry().publishDirectives(static_cast<int>(s), it->second);
                        break;
                    }
                }
            }
        }
    } else {
        GlobalTasteState remoteTaste;
        if (instanceNode.fetchTasteState(remoteTaste)) {
            currentTasteState = remoteTaste;
        }
    }

    // Fetch incoming directives for this instance and update SolverStage
    InstanceDirectives myDirectives;
    if (instanceNode.fetchDirectives(myDirectives)) {
        solverStage.updateDirectives(myDirectives, nowMs);
    } else {
        if (currentTasteState.sessionMode != SessionMode::FreezeAll) {
            solverStage.checkStaleness(nowMs);
        }
    }

    // Apply Taste & Directives to role DSP
    if (currentRole == PluginRole::Source) {
        playbookStage.setLocalTailEnabled(localTailEnabled.load(std::memory_order_relaxed));
        playbookStage.applyTasteDirectives(myDirectives, currentTasteState);

        // Step 5: Profile Learning proposal logic for Source tracks
        if (analyzer.getInstantRmsDb() > -50.0f) {
            timeActiveSignalSeconds += 0.066f;
        }

        bool hasManualAdjustment = (std::abs(moreLessDb.load(std::memory_order_relaxed)) > 0.05f) ||
                                   (std::abs(sendTrimDb.load(std::memory_order_relaxed)) > 0.05f) ||
                                   (playbookStage.getWarmthDrive() > 0.05f) ||
                                   (std::abs(playbookStage.getCharacterTiltDb()) > 0.05f);

        if (hasManualAdjustment && timeActiveSignalSeconds >= 10.0f && (nowMs >= lastProposalSentMs + 30000)) {
            myProposalSeq++;
            lastProposalSentMs = nowMs;
        }
    } else if (currentRole == PluginRole::Bus) {
        busStage.applyTasteDirectives(currentTasteState);
    } else if (currentRole == PluginRole::Master) {
        masterChain.setTasteTonalDelta(currentTasteState.masterLowShelfDb, currentTasteState.masterPresenceDb, currentTasteState.masterAirShelfDb);
    }

    // Synchronize local manual overrides
    uint32_t overrides = ManualOverride::None;
    if (isFrozen.load(std::memory_order_relaxed)) overrides |= ManualOverride::Freeze;
    if (safeChain.isHpfManual() || safeChain.isLpfManual()) overrides |= ManualOverride::EQ;
    solverStage.setManualOverrides(overrides);
    solverStage.setFrozen(isFrozen.load(std::memory_order_relaxed));

    // Publish Telemetry to Shared Registry at 15 Hz
    ++procTimerTick;
    if (procTimerTick == 1 || procTimerTick == 2 || procTimerTick == 5 || procTimerTick == 20 || (procTimerTick % 100 == 0)) {
        traceLog("processor timerCallback (tick %d): role=%d", procTimerTick, static_cast<int>(currentRole));
    }

    InstanceTelemetry telemetry;
    try {
        analyzer.updateAnalysis(telemetry);
    } catch (const std::exception& e) {
        traceLog("analyzer.updateAnalysis std::exception: %s", e.what());
    } catch (...) {
        traceLog("analyzer.updateAnalysis unknown exception caught");
    }
    telemetry.destination = masterChain.getDestination();
    telemetry.busType = (currentRole == PluginRole::Bus) ? busStage.getBusType() : BusType::Generic;
    telemetry.sideHint = currentParseResult.side;
    telemetry.instrumentType = resolvedInstrument;
    telemetry.instrumentConfidence = resolvedConfidence;
    telemetry.identitySource = resolvedSource;
    telemetry.currentTrimDb = safeChain.getTotalTrimDb();
    telemetry.autoTrimAtLimit = safeChain.isAutoTrimAtLimit();
    telemetry.currentRideOffsetDb = solverStage.getActiveRideDb();
    telemetry.momentaryLufs = loudnessMeter.getMomentaryLufs();
    telemetry.shortTermLufs = loudnessMeter.getShortTermLufs();
    telemetry.integratedLufs = loudnessMeter.getIntegratedLufs();
    telemetry.limiterGrDb = (currentRole == PluginRole::Master) ? masterChain.getLimiterGainReductionDb() : 0.0f;
    telemetry.tonalCorrectionDb = (currentRole == PluginRole::Master) ? masterChain.getTonalCorrectionDb() : 0.0f;
    telemetry.narrowbandWarning = (currentRole == PluginRole::Master) && masterChain.isNarrowbandWarningActive();

    // MVP 5 Telemetry fields
    telemetry.moreLessDb = moreLessDb.load(std::memory_order_relaxed);
    telemetry.localTailEnabled = localTailEnabled.load(std::memory_order_relaxed);
    telemetry.localSendTrimDb = sendTrimDb.load(std::memory_order_relaxed);

    telemetry.proposalSequence = myProposalSeq;
    telemetry.proposalRideDelta = moreLessDb.load(std::memory_order_relaxed);
    telemetry.proposalWarmthScale = playbookStage.getWarmthDrive();
    telemetry.proposalSendTrimDb = sendTrimDb.load(std::memory_order_relaxed);
    telemetry.proposalCharacterTiltDb = playbookStage.getCharacterTiltDb();

    if (currentRole == PluginRole::Bus) {
        telemetry.currentCompGrDb = busStage.getGlueGainReductionDb();
    } else if (currentRole == PluginRole::Master) {
        telemetry.currentCompGrDb = masterChain.getGlueGainReductionDb();
        masterChain.updateWorkerLoudness(loudnessMeter.getIntegratedLufs(), loudnessMeter.getShortTermLufs(), analyzer.getInstantRmsDb(), telemetry.spectrumBands);
    } else {
        telemetry.currentCompGrDb = safeChain.getCompGainReductionDb();
    }

    telemetry.solverCutsActive = solverStage.isSolverActive() && (
        solverStage.getActiveCut(0).enabled ||
        solverStage.getActiveCut(1).enabled ||
        solverStage.getActiveCut(2).enabled ||
        solverStage.getActiveCut(3).enabled
    );
    telemetry.solverDuckActive = solverStage.isSolverActive() && (solverStage.getActiveDuckDb() < -0.1f);
    telemetry.isBypassed = isBypassed.load(std::memory_order_relaxed);
    telemetry.isFrozen = isFrozen.load(std::memory_order_relaxed);

    instanceNode.setInstrumentType(resolvedInstrument, resolvedConfidence);
    instanceNode.publishTelemetry(telemetry);
}

void AIMixerAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    state.setProperty("trackName", juce::String(trackName), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AIMixerAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        if (auto* roleParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("role"))) {
            currentRole = static_cast<PluginRole>(roleParam->getIndex());
            instanceNode.setRole(currentRole);
            applyRoleLatency();
        }
        if (apvts.state.hasProperty("trackName")) {
            trackName = apvts.state.getProperty("trackName").toString().toStdString();
            instanceNode.setTrackName(trackName);
            currentParseResult = TrackNameParser::parse(trackName);
        }
    }
}

juce::AudioProcessorEditor* AIMixerAudioProcessor::createEditor() {
    return new AIMixerAudioProcessorEditor(*this);
}

} // namespace aimixer

// Factory function
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    aimixer::traceLog("createPluginFilter entry");
    try {
        return new aimixer::AIMixerAudioProcessor();
    } catch (const std::exception& e) {
        aimixer::traceLog("FATAL: createPluginFilter caught exception: %s", e.what());
        return nullptr;
    } catch (...) {
        aimixer::traceLog("FATAL: createPluginFilter caught unknown exception");
        return nullptr;
    }
}
