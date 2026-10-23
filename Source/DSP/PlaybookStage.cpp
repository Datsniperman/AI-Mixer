#include "PlaybookStage.h"

namespace aimixer {

GatePlaybookConfig PlaybookStage::getGateConfigForInstrument(InstrumentType type) noexcept {
    switch (type) {
        case InstrumentType::Kick:
        case InstrumentType::Snare:
        case InstrumentType::Toms:
            return { true, true, 12.0f, -18.0f, 80.0f, 60.0f }; // Drums gate: +12dB above floor, -18dB floor, 80ms hold, 60ms rel
        case InstrumentType::PastorSpeaking:
        case InstrumentType::LeadVocal:
            return { true, false, 8.0f, -9.0f, 150.0f, 120.0f }; // Voice downward expander: +8dB above floor, -9dB floor, 150ms hold, 120ms rel
        case InstrumentType::ElectricGuitar:
        case InstrumentType::AcousticGuitar:
            return { true, false, 6.0f, -9.0f, 120.0f, 100.0f }; // Guitar noise expander: +6dB above floor, -9dB floor, 120ms hold, 100ms rel
        default:
            return { false, false, 0.0f, 0.0f, 0.0f, 0.0f }; // OFF
    }
}

CharacterEqPreset PlaybookStage::getCharacterEqPreset(InstrumentType type) noexcept {
    CharacterEqPreset p;
    switch (type) {
        case InstrumentType::LeadVocal:
            p.filters[0] = { CharacterFilterType::Peak, 3500.0f, 1.5f, 1.0f };
            p.filters[1] = { CharacterFilterType::HighShelf, 10000.0f, 1.0f, 0.707f };
            p.filters[2] = { CharacterFilterType::Peak, 300.0f, -1.5f, 1.0f };
            break;
        case InstrumentType::PastorSpeaking:
            p.filters[0] = { CharacterFilterType::Peak, 2500.0f, 2.0f, 1.2f };
            p.filters[1] = { CharacterFilterType::Peak, 250.0f, -1.0f, 1.0f };
            break;
        case InstrumentType::BackgroundVocal:
        case InstrumentType::Choir:
            p.filters[0] = { CharacterFilterType::Peak, 3000.0f, -1.0f, 1.4f };
            p.filters[1] = { CharacterFilterType::HighShelf, 10000.0f, 1.0f, 0.707f };
            break;
        case InstrumentType::Kick:
            p.filters[0] = { CharacterFilterType::LowShelf, 60.0f, 1.5f, 0.707f };
            p.filters[1] = { CharacterFilterType::Peak, 3000.0f, 1.0f, 1.4f };
            break;
        case InstrumentType::Snare:
            p.filters[0] = { CharacterFilterType::Peak, 200.0f, 1.5f, 1.2f };
            p.filters[1] = { CharacterFilterType::Peak, 5000.0f, 1.0f, 1.4f };
            break;
        case InstrumentType::Bass:
            p.filters[0] = { CharacterFilterType::LowShelf, 60.0f, 1.0f, 0.707f };
            p.filters[1] = { CharacterFilterType::Peak, 300.0f, -1.0f, 1.4f };
            break;
        case InstrumentType::AcousticGuitar:
            p.filters[0] = { CharacterFilterType::HighShelf, 8000.0f, 1.0f, 0.707f };
            p.filters[1] = { CharacterFilterType::Peak, 350.0f, -1.0f, 1.2f };
            break;
        case InstrumentType::ElectricGuitar:
            p.filters[0] = { CharacterFilterType::Peak, 400.0f, -1.0f, 1.2f };
            p.filters[1] = { CharacterFilterType::Peak, 7500.0f, -1.5f, 1.4f }; // anti-fizz
            break;
        case InstrumentType::Piano:
            p.filters[0] = { CharacterFilterType::HighShelf, 8000.0f, 1.0f, 0.707f };
            break;
        case InstrumentType::Keys:
            p.filters[0] = { CharacterFilterType::HighShelf, 8000.0f, 0.5f, 0.707f };
            break;
        case InstrumentType::Pad:
            p.filters[0] = { CharacterFilterType::Peak, 400.0f, -1.0f, 1.2f };
            p.filters[1] = { CharacterFilterType::HighShelf, 9000.0f, 0.5f, 0.707f };
            break;
        case InstrumentType::Synth:
            p.filters[0] = { CharacterFilterType::Peak, 350.0f, -1.0f, 1.2f };
            break;
        case InstrumentType::Overheads:
            p.filters[0] = { CharacterFilterType::HighShelf, 10000.0f, 1.0f, 0.707f };
            break;
        case InstrumentType::Room:
            p.filters[0] = { CharacterFilterType::Peak, 300.0f, -1.5f, 1.2f }; // distance mud
            break;
        case InstrumentType::Tracks:
        case InstrumentType::HiHat:
        case InstrumentType::Toms:
        case InstrumentType::Click:
        case InstrumentType::Unknown:
        default:
            break; // flat
    }
    return p;
}

float PlaybookStage::getSaturationDriveForInstrument(InstrumentType type) noexcept {
    switch (type) {
        case InstrumentType::Bass:           return 0.25f;
        case InstrumentType::ElectricGuitar: return 0.20f;
        case InstrumentType::Snare:          return 0.15f;
        case InstrumentType::Kick:           return 0.15f;
        case InstrumentType::LeadVocal:      return 0.10f; // Warmth
        case InstrumentType::PastorSpeaking: return 0.05f; // Subtle
        default:                             return 0.0f;  // OFF
    }
}

float PlaybookStage::getTransientBoostForInstrument(InstrumentType type) noexcept {
    switch (type) {
        case InstrumentType::Kick:  return 2.0f; // +2 dB transient emphasis
        case InstrumentType::Snare: return 1.5f; // +1.5 dB
        case InstrumentType::Toms:  return 1.0f; // +1.0 dB
        default:                    return 0.0f; // OFF
    }
}

PanWidthConfig PlaybookStage::getPanWidthConfig(InstrumentType type, SideHint side) noexcept {
    float panOffset = (side == SideHint::Left) ? -0.4f : (side == SideHint::Right ? 0.4f : 0.0f);

    switch (type) {
        case InstrumentType::LeadVocal:
            return { 0.0f, 1.0f, false };
        case InstrumentType::PastorSpeaking:
            return { 0.0f, 0.8f, false }; // narrower, focused
        case InstrumentType::BackgroundVocal:
        case InstrumentType::Choir:
            return { -0.15f + (side == SideHint::Left ? -0.2f : (side == SideHint::Right ? 0.35f : 0.0f)), 1.3f, false };
        case InstrumentType::Kick:
            return { 0.0f, 0.6f, true }; // Mono <120 Hz
        case InstrumentType::Bass:
            return { 0.0f, 0.5f, true }; // Mono <120 Hz
        case InstrumentType::Snare:
            return { 0.0f, 0.8f, false };
        case InstrumentType::Toms:
            return { (side == SideHint::Left ? -0.3f : (side == SideHint::Right ? 0.3f : 0.0f)), 1.0f, false };
        case InstrumentType::HiHat:
            return { (side == SideHint::Left ? -0.3f : (side == SideHint::Right ? 0.1f : -0.2f)), 1.0f, false };
        case InstrumentType::Overheads:
            return { 0.0f, 1.5f, false };
        case InstrumentType::AcousticGuitar:
            return { (side == SideHint::Left ? -0.4f : (side == SideHint::Right ? 0.4f : -0.25f)), 1.1f, false };
        case InstrumentType::ElectricGuitar:
            return { (side != SideHint::Centre ? panOffset : 0.0f), 1.1f, false };
        case InstrumentType::Piano:
            return { 0.0f, 1.4f, false };
        case InstrumentType::Keys:
            return { 0.15f, 1.4f, false };
        case InstrumentType::Pad:
            return { 0.0f, 1.7f, true }; // Mono <120 Hz
        case InstrumentType::Synth:
            return { (side == SideHint::Left ? -0.3f : (side == SideHint::Right ? 0.3f : 0.0f)), 1.2f, false };
        case InstrumentType::Tracks:
            return { 0.0f, 1.2f, true }; // Mono <120 Hz
        case InstrumentType::Room:
            return { 0.0f, 1.6f, false };
        case InstrumentType::Click:
        case InstrumentType::Unknown:
        default:
            return { 0.0f, 1.0f, false };
    }
}

PlaybookStage::PlaybookStage() {
    activeEqPreset = getCharacterEqPreset(InstrumentType::Unknown);
}

PlaybookStage::~PlaybookStage() = default;

void PlaybookStage::prepare(const juce::dsp::ProcessSpec& spec) {
    currentSampleRate = (spec.sampleRate > 0.0) ? spec.sampleRate : 48000.0;

    // Gate time constants
    gateAttackCoeff = std::exp(-1.0f / (0.0015f * static_cast<float>(currentSampleRate))); // 1.5ms attack
    gateReleaseCoeff = std::exp(-1.0f / (0.080f * static_cast<float>(currentSampleRate)));  // default 80ms rel

    // De-Esser time constants
    deEssSmoothAttackCoeff = std::exp(-1.0f / (0.005f * static_cast<float>(currentSampleRate))); // 5ms attack
    deEssReleaseCoeff = std::exp(-1.0f / (0.040f * static_cast<float>(currentSampleRate)));      // 40ms release

    // Transient time constants
    transFastAttCoeff = std::exp(-1.0f / (0.001f * static_cast<float>(currentSampleRate))); // 1ms
    transFastRelCoeff = std::exp(-1.0f / (0.020f * static_cast<float>(currentSampleRate))); // 20ms
    transSlowAttCoeff = std::exp(-1.0f / (0.015f * static_cast<float>(currentSampleRate))); // 15ms
    transSlowRelCoeff = std::exp(-1.0f / (0.080f * static_cast<float>(currentSampleRate))); // 80ms

    // Local rider buffer size
    localRmsWindowSamples = static_cast<int>(0.400f * static_cast<float>(currentSampleRate));

    // Shared FX engines
    reverbEngine.prepare(spec);
    delayEngine.prepare(spec);
    int maxBlock = std::max(64, static_cast<int>(spec.maximumBlockSize));
    wetRevL.assign(maxBlock, 0.0f);
    wetRevR.assign(maxBlock, 0.0f);
    wetDlyL.assign(maxBlock, 0.0f);
    wetDlyR.assign(maxBlock, 0.0f);

    updateCharacterFilters();
    updateCharacterTiltFilter();
    updateDeEsserFilter(deEsserCenterFreqHz);
    updateBassMonoFilters();
    reset();
}

void PlaybookStage::reset() {
    gateSmoothedGain = 1.0f;
    gateIsOpen.store(true, std::memory_order_relaxed);
    currentGateGrDb.store(0.0f, std::memory_order_relaxed);
    gateHoldSamplesRemaining = 0;
    gateMinClosedSamplesRemaining = 0;

    for (int k = 0; k < 3; ++k) {
        charFiltersLeft[k].reset();
        charFiltersRight[k].reset();
    }
    charTiltFilterLeft.reset();
    charTiltFilterRight.reset();

    deEssDetectBandLeft.reset();
    deEssDetectBandRight.reset();
    deEssCutFilterLeft.reset();
    deEssCutFilterRight.reset();
    deEssEnvelope = 0.0f;
    currentDeEssGrDb.store(0.0f, std::memory_order_relaxed);

    transFastEnvL = transFastEnvR = 0.0f;
    transSlowEnvL = transSlowEnvR = 0.0f;

    localRmsAccumulator = 0.0f;
    localRmsSampleCount = 0;
    localRollingAverageRms = -18.0f;
    currentLocalRideDb.store(0.0f, std::memory_order_relaxed);

    bassMonoLpfLeft.reset();
    bassMonoLpfRight.reset();
    bassMonoHpfLeft.reset();
    bassMonoHpfRight.reset();

    reverbEngine.reset();
    delayEngine.reset();
}

void PlaybookStage::updateCharacterFilters() {
    if (currentSampleRate <= 0.0) return;

    for (int k = 0; k < 3; ++k) {
        const auto& def = activeEqPreset.filters[k];
        if (def.type == CharacterFilterType::Off || std::abs(def.gainDb) < 0.05f) {
            auto flatCoefs = FilterCoefs::makePeakFilter(currentSampleRate, 1000.0f, 1.0f, 1.0f);
            charFiltersLeft[k].coefficients = flatCoefs;
            charFiltersRight[k].coefficients = flatCoefs;
            continue;
        }

        float safeFreq = std::clamp(def.freqHz, 20.0f, static_cast<float>(currentSampleRate * 0.48));
        float safeGain = std::clamp(def.gainDb, -2.5f, 2.5f);
        float linearGain = juce::Decibels::decibelsToGain(safeGain);
        float safeQ = std::clamp(def.q, 0.5f, 4.0f);

        Filter::CoefficientsPtr coefs;
        if (def.type == CharacterFilterType::LowShelf) {
            coefs = FilterCoefs::makeLowShelf(currentSampleRate, safeFreq, safeQ, linearGain);
        } else if (def.type == CharacterFilterType::HighShelf) {
            coefs = FilterCoefs::makeHighShelf(currentSampleRate, safeFreq, safeQ, linearGain);
        } else {
            coefs = FilterCoefs::makePeakFilter(currentSampleRate, safeFreq, safeQ, linearGain);
        }

        charFiltersLeft[k].coefficients = coefs;
        charFiltersRight[k].coefficients = coefs;
    }
}

void PlaybookStage::updateDeEsserFilter(float centroidHz) {
    if (currentSampleRate <= 0.0) return;

    // Center tuned between 5-9 kHz based on spectral centroid
    float tunedFreq = 6500.0f;
    if (centroidHz > 3000.0f) {
        tunedFreq = std::clamp(centroidHz * 1.5f, 5000.0f, 9000.0f);
    }
    deEsserCenterFreqHz = tunedFreq;

    // Detector bandpass
    auto bpCoefs = FilterCoefs::makeBandPass(currentSampleRate, tunedFreq, 1.4f);
    deEssDetectBandLeft.coefficients = bpCoefs;
    deEssDetectBandRight.coefficients = bpCoefs;

    // Initial flat cut biquad
    auto peakCoefs = FilterCoefs::makePeakFilter(currentSampleRate, tunedFreq, 1.4f, 1.0f);
    deEssCutFilterLeft.coefficients = peakCoefs;
    deEssCutFilterRight.coefficients = peakCoefs;
}

void PlaybookStage::updateBassMonoFilters() {
    if (currentSampleRate <= 0.0) return;

    // 2nd-order Linkwitz-Riley crossover at 120 Hz (Q = 0.707)
    const float xoverHz = 120.0f;
    auto lpf = FilterCoefs::makeLowPass(currentSampleRate, xoverHz, 0.7071f);
    auto hpf = FilterCoefs::makeHighPass(currentSampleRate, xoverHz, 0.7071f);

    bassMonoLpfLeft.coefficients = lpf;
    bassMonoLpfRight.coefficients = lpf;
    bassMonoHpfLeft.coefficients = hpf;
    bassMonoHpfRight.coefficients = hpf;
}

void PlaybookStage::updateCharacterTiltFilter() {
    if (currentSampleRate <= 0.0) return;
    float tiltDb = activeCharacterTiltDb.load(std::memory_order_relaxed);
    if (std::abs(tiltDb) < 0.05f) {
        auto flat = FilterCoefs::makePeakFilter(currentSampleRate, 8000.0f, 1.0f, 1.0f);
        charTiltFilterLeft.coefficients = flat;
        charTiltFilterRight.coefficients = flat;
        return;
    }
    float safeGain = std::clamp(tiltDb, -4.0f, 4.0f);
    auto shelf = FilterCoefs::makeHighShelf(currentSampleRate, 8000.0f, 0.7071f, juce::Decibels::decibelsToGain(safeGain));
    charTiltFilterLeft.coefficients = shelf;
    charTiltFilterRight.coefficients = shelf;
}

void PlaybookStage::applyTasteDirectives(const InstanceDirectives& dir, const GlobalTasteState& taste) {
    if (dir.sessionMode == SessionMode::FreezeAll || isFrozen.load(std::memory_order_relaxed)) {
        return;
    }

    // Warmth Target
    if (!warmthManualOverride.load(std::memory_order_relaxed) && !(manualOverrides.load(std::memory_order_relaxed) & ManualOverride::Warmth)) {
        float baseDrive = getSaturationDriveForInstrument(currentInstrument);
        activeWarmthDrive.store(std::clamp(baseDrive * dir.warmthTarget, 0.0f, 0.60f), std::memory_order_relaxed);
    }

    // Character Tilt
    if (!(manualOverrides.load(std::memory_order_relaxed) & ManualOverride::Character)) {
        activeCharacterTiltDb.store(dir.characterTiltDb, std::memory_order_relaxed);
        updateCharacterTiltFilter();
    }

    // Shared FX sends
    activeReverbSendDb.store(dir.reverbSendDb, std::memory_order_relaxed);
    activeDelaySendDb.store(dir.delaySendDb, std::memory_order_relaxed);

    // Engine parameters
    reverbEngine.setReverbType(taste.reverbType);
    reverbEngine.setDecaySec(taste.reverbDecaySec);
    reverbEngine.setPredelayMs(taste.reverbPredelayMs);
    reverbEngine.setDamping(taste.reverbDamping);

    delayEngine.setSubdivision(taste.delaySubdivision);
    delayEngine.setFeedback(taste.delayFeedback);
    delayEngine.setPingPong(taste.delayPingPong);
}

void PlaybookStage::setInstrumentIdentity(InstrumentType type, SideHint side) {
    currentInstrument = type;
    currentSide = side;

    // 1b. Gate
    gateConfig = getGateConfigForInstrument(type);
    if (currentSampleRate > 0.0) {
        gateReleaseCoeff = std::exp(-1.0f / (std::max(0.010f, gateConfig.releaseMs * 0.001f) * static_cast<float>(currentSampleRate)));
        gateHoldSamplesTotal = static_cast<float>(gateConfig.holdMs * 0.001f * currentSampleRate);
        gateMinClosedSamplesTotal = static_cast<float>(0.050f * currentSampleRate); // 50ms min closed
    }
    if (!gateManualOverride.load(std::memory_order_relaxed)) {
        targetAutoThresholdDb = lastNoiseFloorRmsDb + gateConfig.thresholdOffsetDb;
        activeGateThresholdDb.store(targetAutoThresholdDb, std::memory_order_relaxed);
    }

    // 1c. Character EQ
    if (!(manualOverrides.load(std::memory_order_relaxed) & ManualOverride::Character)) {
        activeEqPreset = getCharacterEqPreset(type);
        updateCharacterFilters();
    }

    // 1d. De-Esser
    if (type == InstrumentType::LeadVocal || type == InstrumentType::PastorSpeaking || type == InstrumentType::BackgroundVocal || type == InstrumentType::Choir) {
        deEsserEnabled = true;
        deEsserMaxCapDb = -6.0f;
    } else if (type == InstrumentType::Overheads || type == InstrumentType::HiHat) {
        deEsserEnabled = true;
        deEsserMaxCapDb = -3.0f; // Half depth "harshness"
    } else {
        deEsserEnabled = false;
        deEsserMaxCapDb = 0.0f;
    }

    // 1e. Saturation / Warmth
    if (!warmthManualOverride.load(std::memory_order_relaxed)) {
        activeWarmthDrive.store(getSaturationDriveForInstrument(type), std::memory_order_relaxed);
    }

    // 1f. Transient Stage
    activeTransientBoostDb.store(getTransientBoostForInstrument(type), std::memory_order_relaxed);

    // 1g. Local Rider
    if (type == InstrumentType::LeadVocal || type == InstrumentType::PastorSpeaking) {
        localRiderEnabled = true;
        localRiderWindowMs = (type == InstrumentType::PastorSpeaking) ? 300.0f : 400.0f;
        localRmsWindowSamples = static_cast<int>(localRiderWindowMs * 0.001f * static_cast<float>(currentSampleRate));
    } else {
        localRiderEnabled = false;
        currentLocalRideDb.store(0.0f, std::memory_order_relaxed);
    }

    // 1h. Pan & Width
    panWidthConfig = getPanWidthConfig(type, side);
    if (!panWidthManualOverride.load(std::memory_order_relaxed)) {
        activePan.store(panWidthConfig.defaultPan, std::memory_order_relaxed);
        activeWidth.store(panWidthConfig.defaultWidth, std::memory_order_relaxed);
    }
}

void PlaybookStage::updateWorkerAnalysis(float instantRmsDb, float noiseFloorRmsDb, float spectralCentroidHz) {
    lastNoiseFloorRmsDb = noiseFloorRmsDb;

    // 1b. Gate threshold adaptation (at most 1 dB/sec)
    if (gateConfig.enabled && !gateManualOverride.load(std::memory_order_relaxed)) {
        float rawTarget = noiseFloorRmsDb + gateConfig.thresholdOffsetDb;
        float currentThresh = activeGateThresholdDb.load(std::memory_order_relaxed);
        float maxStep = 0.20f; // at ~5 Hz = 1.0 dB/sec
        float step = std::clamp(rawTarget - currentThresh, -maxStep, maxStep);
        activeGateThresholdDb.store(currentThresh + step, std::memory_order_relaxed);
    }

    // 1d. De-Esser centroid tuning
    if (deEsserEnabled && spectralCentroidHz > 1000.0f) {
        updateDeEsserFilter(spectralCentroidHz);
    }

    // 1g. Local Rider 10-sec rolling average tracking
    if (localRiderEnabled && instantRmsDb > -50.0f) {
        localRollingAverageRms = 0.98f * localRollingAverageRms + 0.02f * instantRmsDb;
        float desiredGainDb = std::clamp(localRollingAverageRms - instantRmsDb, -3.0f, 3.0f);
        localRiderTargetDb = desiredGainDb;
    }
}

void PlaybookStage::setManualGateThreshold(float threshDb) noexcept {
    gateManualOverride.store(true, std::memory_order_relaxed);
    activeGateThresholdDb.store(threshDb, std::memory_order_relaxed);
}

void PlaybookStage::resetGateToAuto() noexcept {
    gateManualOverride.store(false, std::memory_order_relaxed);
    activeGateThresholdDb.store(lastNoiseFloorRmsDb + gateConfig.thresholdOffsetDb, std::memory_order_relaxed);
}

void PlaybookStage::setManualWarmthDrive(float drive) noexcept {
    warmthManualOverride.store(true, std::memory_order_relaxed);
    activeWarmthDrive.store(std::clamp(drive, 0.0f, 0.5f), std::memory_order_relaxed);
}

void PlaybookStage::resetWarmthToAuto() noexcept {
    warmthManualOverride.store(false, std::memory_order_relaxed);
    activeWarmthDrive.store(getSaturationDriveForInstrument(currentInstrument), std::memory_order_relaxed);
}

void PlaybookStage::setManualPan(float pan) noexcept {
    panWidthManualOverride.store(true, std::memory_order_relaxed);
    activePan.store(std::clamp(pan, -1.0f, 1.0f), std::memory_order_relaxed);
}

void PlaybookStage::setManualWidth(float width) noexcept {
    panWidthManualOverride.store(true, std::memory_order_relaxed);
    activeWidth.store(std::clamp(width, 0.0f, 2.0f), std::memory_order_relaxed);
}

void PlaybookStage::resetPanWidthToAuto() noexcept {
    panWidthManualOverride.store(false, std::memory_order_relaxed);
    activePan.store(panWidthConfig.defaultPan, std::memory_order_relaxed);
    activeWidth.store(panWidthConfig.defaultWidth, std::memory_order_relaxed);
}

void PlaybookStage::process(juce::AudioBuffer<float>& buffer) noexcept {
    if (currentInstrument == InstrumentType::Click) return; // Pass-through

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    bool frozen = isFrozen.load(std::memory_order_relaxed);
    uint32_t overrides = manualOverrides.load(std::memory_order_relaxed);

    float* leftData = buffer.getWritePointer(0);
    float* rightData = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    const bool gateOn = gateConfig.enabled && !gateBypassed.load(std::memory_order_relaxed);
    const float gateThresh = activeGateThresholdDb.load(std::memory_order_relaxed);
    const float closeThresh = gateThresh - 3.0f; // 3 dB hysteresis
    const float gateRangeLinear = juce::Decibels::decibelsToGain(gateConfig.maxRangeDb);

    const bool deEssOn = deEsserEnabled && !deEsserBypassed.load(std::memory_order_relaxed) && !(overrides & ManualOverride::DeEss);
    const float warmthDrive = warmthBypassed.load(std::memory_order_relaxed) ? 0.0f : activeWarmthDrive.load(std::memory_order_relaxed);
    const float warmthComp = 1.0f / (1.0f + 0.6f * warmthDrive);

    const float transBoostDb = transientBypassed.load(std::memory_order_relaxed) ? 0.0f : activeTransientBoostDb.load(std::memory_order_relaxed);
    const float maxTransGain = juce::Decibels::decibelsToGain(transBoostDb);

    const bool localRiderOn = localRiderEnabled && !localRiderBypassed.load(std::memory_order_relaxed) && !frozen && !(overrides & ManualOverride::LevelRide);

    const bool panWidthOn = !panWidthBypassed.load(std::memory_order_relaxed);
    const float panVal = std::clamp(activePan.load(std::memory_order_relaxed), -1.0f, 1.0f);
    const float widthVal = std::clamp(activeWidth.load(std::memory_order_relaxed), 0.0f, 2.0f);
    const float panAngle = 0.25f * juce::MathConstants<float>::pi * (panVal + 1.0f);
    const float panGainL = std::cos(panAngle) * 1.41421356f;
    const float panGainR = std::sin(panAngle) * 1.41421356f;
    const bool doBassMono = panWidthConfig.bassMonoBelow120Hz && panWidthOn && (numChannels > 1);

    float maxGateGrDbThisBlock = 0.0f;
    float maxDeEssGrDbThisBlock = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        float left = leftData[i];
        float right = rightData ? rightData[i] : left;

        // ----------------------------------------------------------------------
        // 1b. Gate / Expander Stage
        // ----------------------------------------------------------------------
        if (gateOn) {
            float det = 0.5f * (std::abs(left) + std::abs(right));
            float detDb = (det > 1e-5f) ? juce::Decibels::gainToDecibels(det) : -100.0f;

            float targetGain = 1.0f;
            if (detDb >= gateThresh) {
                if (gateMinClosedSamplesRemaining <= 0) {
                    gateIsOpen.store(true, std::memory_order_relaxed);
                    gateHoldSamplesRemaining = static_cast<int>(gateHoldSamplesTotal);
                }
            } else if (detDb < closeThresh) {
                if (gateHoldSamplesRemaining > 0) {
                    gateHoldSamplesRemaining--;
                } else {
                    if (gateIsOpen.load(std::memory_order_relaxed)) {
                        gateIsOpen.store(false, std::memory_order_relaxed);
                        gateMinClosedSamplesRemaining = static_cast<int>(gateMinClosedSamplesTotal);
                    }
                }
            }

            if (gateMinClosedSamplesRemaining > 0) {
                gateMinClosedSamplesRemaining--;
            }

            if (!gateIsOpen.load(std::memory_order_relaxed)) {
                targetGain = gateRangeLinear;
            }

            if (targetGain > gateSmoothedGain) {
                gateSmoothedGain = gateAttackCoeff * gateSmoothedGain + (1.0f - gateAttackCoeff) * targetGain;
            } else {
                gateSmoothedGain = gateReleaseCoeff * gateSmoothedGain + (1.0f - gateReleaseCoeff) * targetGain;
            }

            float currentGrDb = -juce::Decibels::gainToDecibels(std::clamp(gateSmoothedGain, 1e-4f, 1.0f));
            if (currentGrDb > maxGateGrDbThisBlock) maxGateGrDbThisBlock = currentGrDb;

            left *= gateSmoothedGain;
            if (rightData) right *= gateSmoothedGain;
        }

        // ----------------------------------------------------------------------
        // 1c. Character EQ Stage
        // ----------------------------------------------------------------------
        if (!eqBypassed.load(std::memory_order_relaxed) && !(overrides & ManualOverride::Character)) {
            for (int k = 0; k < 3; ++k) {
                left = charFiltersLeft[k].processSample(left);
                if (rightData) right = charFiltersRight[k].processSample(right);
            }
        }

        // ----------------------------------------------------------------------
        // 1d. De-Esser / Harshness Stage
        // ----------------------------------------------------------------------
        if (deEssOn) {
            float sibilanceSampleL = deEssDetectBandLeft.processSample(left);
            float sibilanceSampleR = rightData ? deEssDetectBandRight.processSample(right) : sibilanceSampleL;
            float sibEnergy = 0.5f * (std::abs(sibilanceSampleL) + std::abs(sibilanceSampleR));

            float targetCutDb = 0.0f;
            float sibDb = (sibEnergy > 1e-5f) ? juce::Decibels::gainToDecibels(sibEnergy) : -100.0f;
            const float deEssThreshDb = -24.0f;
            if (sibDb > deEssThreshDb) {
                float excessDb = sibDb - deEssThreshDb;
                targetCutDb = std::clamp(excessDb * 0.6f, 0.0f, std::abs(deEsserMaxCapDb));
            }

            // Ballistic envelope
            if (targetCutDb > deEssEnvelope) {
                deEssEnvelope = deEssSmoothAttackCoeff * deEssEnvelope + (1.0f - deEssSmoothAttackCoeff) * targetCutDb;
            } else {
                deEssEnvelope = deEssReleaseCoeff * deEssEnvelope + (1.0f - deEssReleaseCoeff) * targetCutDb;
            }

            if (deEssEnvelope > maxDeEssGrDbThisBlock) maxDeEssGrDbThisBlock = deEssEnvelope;

            if (deEssEnvelope > 0.05f) {
                float cutGainLinear = juce::Decibels::decibelsToGain(-deEssEnvelope);
                auto dynamicCoefs = FilterCoefs::makePeakFilter(currentSampleRate, deEsserCenterFreqHz, 1.4f, cutGainLinear);
                deEssCutFilterLeft.coefficients = dynamicCoefs;
                deEssCutFilterRight.coefficients = dynamicCoefs;

                left = deEssCutFilterLeft.processSample(left);
                if (rightData) right = deEssCutFilterRight.processSample(right);
            }
        }

        // ----------------------------------------------------------------------
        // 1e. Saturation / Warmth Stage (tanh waveshaper)
        // ----------------------------------------------------------------------
        if (warmthDrive > 0.001f) {
            float xL = left * (1.0f + warmthDrive);
            float xR = right * (1.0f + warmthDrive);
            left = std::tanh(xL) * warmthComp;
            if (rightData) right = std::tanh(xR) * warmthComp;
        }

        // ----------------------------------------------------------------------
        // 1f. Transient Stage (Kick, Snare, Toms attack emphasis)
        // ----------------------------------------------------------------------
        if (transBoostDb > 0.01f) {
            float inAbsL = std::abs(left);
            float inAbsR = std::abs(right);

            // Fast envelope (1ms att / 20ms rel)
            transFastEnvL = (inAbsL > transFastEnvL) ? (transFastAttCoeff * transFastEnvL + (1.0f - transFastAttCoeff) * inAbsL)
                                                     : (transFastRelCoeff * transFastEnvL + (1.0f - transFastRelCoeff) * inAbsL);
            transFastEnvR = (inAbsR > transFastEnvR) ? (transFastAttCoeff * transFastEnvR + (1.0f - transFastAttCoeff) * inAbsR)
                                                     : (transFastRelCoeff * transFastEnvR + (1.0f - transFastRelCoeff) * inAbsR);

            // Slow envelope (15ms att / 80ms rel)
            transSlowEnvL = (inAbsL > transSlowEnvL) ? (transSlowAttCoeff * transSlowEnvL + (1.0f - transSlowAttCoeff) * inAbsL)
                                                     : (transSlowRelCoeff * transSlowEnvL + (1.0f - transSlowRelCoeff) * inAbsL);
            transSlowEnvR = (inAbsR > transSlowEnvR) ? (transSlowAttCoeff * transSlowEnvR + (1.0f - transSlowAttCoeff) * inAbsR)
                                                     : (transSlowRelCoeff * transSlowEnvR + (1.0f - transSlowRelCoeff) * inAbsR);

            float diffL = std::max(0.0f, transFastEnvL - transSlowEnvL);
            float diffR = std::max(0.0f, transFastEnvR - transSlowEnvR);

            float modL = 1.0f + (maxTransGain - 1.0f) * std::clamp(diffL * 8.0f, 0.0f, 1.0f);
            float modR = 1.0f + (maxTransGain - 1.0f) * std::clamp(diffR * 8.0f, 0.0f, 1.0f);

            // Output peak never exceeds input peak by > 2 dB (maxTransGain)
            left *= modL;
            if (rightData) right *= modR;
        }

        // ----------------------------------------------------------------------
        // 1g. Local Rider Stage (Phrase leveling for voices)
        // ----------------------------------------------------------------------
        if (localRiderOn) {
            float rateLimitStep = 3.0f / (static_cast<float>(currentSampleRate)); // 3.0 dB/sec vocal phrase rider rate
            float curR = currentLocalRideDb.load(std::memory_order_relaxed);
            float step = std::clamp(localRiderTargetDb - curR, -rateLimitStep, rateLimitStep);
            curR += step;
            currentLocalRideDb.store(curR, std::memory_order_relaxed);

            float g = juce::Decibels::decibelsToGain(curR);
            left *= g;
            if (rightData) right *= g;
        }

        // ----------------------------------------------------------------------
        // 1h. Pan / Width & Bass-Mono Stage
        // ----------------------------------------------------------------------
        if (panWidthOn) {
            // Bass-mono below 120 Hz via Linkwitz-Riley crossover
            if (doBassMono) {
                float lowL = bassMonoLpfLeft.processSample(left);
                float lowR = bassMonoLpfRight.processSample(right);
                float highL = bassMonoHpfLeft.processSample(left);
                float highR = bassMonoHpfRight.processSample(right);

                float monoLow = 0.5f * (lowL + lowR);
                left = monoLow + highL;
                right = monoLow + highR;
            }

            // Width via M/S adjustment
            if (rightData) {
                float mid = 0.5f * (left + right);
                float side = 0.5f * (left - right) * widthVal;
                left = mid + side;
                right = mid - side;
            }

            // Constant-power pan
            left *= panGainL;
            if (rightData) right *= panGainR;
        }

        leftData[i] = left;
        if (rightData) rightData[i] = right;
    }

    currentGateGrDb.store(maxGateGrDbThisBlock, std::memory_order_relaxed);
    currentDeEssGrDb.store(maxDeEssGrDbThisBlock, std::memory_order_relaxed);

    // ----------------------------------------------------------------------
    // Character Tilt Shelf (High shelf @ 8 kHz)
    // ----------------------------------------------------------------------
    if (std::abs(activeCharacterTiltDb.load(std::memory_order_relaxed)) >= 0.05f) {
        for (int i = 0; i < numSamples; ++i) {
            leftData[i] = charTiltFilterLeft.processSample(leftData[i]);
            if (rightData) rightData[i] = charTiltFilterRight.processSample(rightData[i]);
        }
    }

    // ----------------------------------------------------------------------
    // 1i. Shared FX (Local Tail Mode)
    // ----------------------------------------------------------------------
    if (localTailEnabled.load(std::memory_order_relaxed)) {
        float revDb = activeReverbSendDb.load(std::memory_order_relaxed);
        float dlyDb = activeDelaySendDb.load(std::memory_order_relaxed);

        if (static_cast<int>(wetRevL.size()) < numSamples) {
            wetRevL.resize(static_cast<size_t>(numSamples), 0.0f);
            wetRevR.resize(static_cast<size_t>(numSamples), 0.0f);
            wetDlyL.resize(static_cast<size_t>(numSamples), 0.0f);
            wetDlyR.resize(static_cast<size_t>(numSamples), 0.0f);
        }

        if (revDb > -50.0f) {
            reverbEngine.process(leftData, rightData ? rightData : leftData, wetRevL.data(), wetRevR.data(), static_cast<size_t>(numSamples));
            float revGain = juce::Decibels::decibelsToGain(revDb);
            for (int i = 0; i < numSamples; ++i) {
                leftData[i] += wetRevL[static_cast<size_t>(i)] * revGain;
                if (rightData) rightData[i] += wetRevR[static_cast<size_t>(i)] * revGain;
            }
        }

        if (dlyDb > -50.0f) {
            delayEngine.process(leftData, rightData ? rightData : leftData, wetDlyL.data(), wetDlyR.data(), static_cast<size_t>(numSamples));
            float dlyGain = juce::Decibels::decibelsToGain(dlyDb);
            for (int i = 0; i < numSamples; ++i) {
                leftData[i] += wetDlyL[static_cast<size_t>(i)] * dlyGain;
                if (rightData) rightData[i] += wetDlyR[static_cast<size_t>(i)] * dlyGain;
            }
        }
    }
}

} // namespace aimixer
