#include "SafeChain.h"
#include <cmath>
#include <algorithm>

namespace aimixer {

SafeChainFilterDefaults getFilterDefaultsForInstrument(InstrumentType type) noexcept {
    switch (type) {
        case InstrumentType::LeadVocal:       return { 80.0f,  20000.0f, true };
        case InstrumentType::PastorSpeaking:  return { 95.0f,  20000.0f, true };
        case InstrumentType::BackgroundVocal: return { 80.0f,  20000.0f, true };
        case InstrumentType::Choir:           return { 80.0f,  20000.0f, true };
        case InstrumentType::Kick:            return { 20.0f,  20000.0f, true };
        case InstrumentType::Snare:           return { 60.0f,  20000.0f, true };
        case InstrumentType::HiHat:           return { 150.0f, 20000.0f, false }; // HiHat: comp off
        case InstrumentType::Toms:            return { 40.0f,  20000.0f, true };
        case InstrumentType::Overheads:       return { 100.0f, 20000.0f, true };
        case InstrumentType::Bass:            return { 25.0f,  8000.0f,  true };
        case InstrumentType::AcousticGuitar:  return { 70.0f,  20000.0f, true };
        case InstrumentType::ElectricGuitar:  return { 65.0f,  20000.0f, true };
        case InstrumentType::Piano:           return { 40.0f,  20000.0f, true };
        case InstrumentType::Keys:            return { 40.0f,  20000.0f, true };
        case InstrumentType::Pad:             return { 35.0f,  12000.0f, false }; // Pad: comp off
        case InstrumentType::Synth:           return { 30.0f,  20000.0f, true };
        case InstrumentType::Tracks:          return { 25.0f,  20000.0f, true };
        case InstrumentType::Room:            return { 60.0f,  10000.0f, false }; // Room: comp off
        case InstrumentType::Click:           return { 20.0f,  20000.0f, false }; // Click: pass-through
        case InstrumentType::Unknown:
        default:                              return { 40.0f,  20000.0f, true };
    }
}

CompPlaybookSettings getCompSettingsForInstrument(InstrumentType type) noexcept {
    switch (type) {
        case InstrumentType::LeadVocal:
            return { true, 3.0f, 3.0f, 120.0f, 4.0f, 6.0f }; // 3:1, 3ms, 120ms, soft (4dB knee), 6dB cap
        case InstrumentType::PastorSpeaking:
            return { true, 4.0f, 5.0f, 200.0f, 4.0f, 8.0f }; // 4:1, 5ms, 200ms, soft, 8dB cap
        case InstrumentType::BackgroundVocal:
        case InstrumentType::Choir:
            return { true, 2.0f, 10.0f, 200.0f, 4.0f, 4.0f }; // 2:1, 10ms, 200ms, soft, 4dB cap
        case InstrumentType::Kick:
            return { true, 3.0f, 1.0f, 80.0f, 2.0f, 5.0f };  // 3:1, 1ms, 80ms, med (2dB), 5dB cap
        case InstrumentType::Snare:
            return { true, 3.0f, 2.0f, 100.0f, 2.0f, 5.0f }; // 3:1, 2ms, 100ms, med, 5dB cap
        case InstrumentType::Toms:
            return { true, 2.5f, 3.0f, 120.0f, 2.0f, 5.0f }; // 2.5:1, 3ms, 120ms, med, 5dB cap
        case InstrumentType::Overheads:
            return { true, 1.5f, 20.0f, 300.0f, 4.0f, 2.0f }; // 1.5:1, 20ms, 300ms, soft, 2dB cap
        case InstrumentType::HiHat:
            return { false, 1.0f, 10.0f, 150.0f, 2.0f, 0.0f }; // HiHat OFF
        case InstrumentType::Bass:
            return { true, 2.5f, 15.0f, 180.0f, 4.0f, 5.0f }; // 2.5:1, 15ms, 180ms, soft, 5dB cap
        case InstrumentType::AcousticGuitar:
            return { true, 2.0f, 8.0f, 150.0f, 4.0f, 4.0f };  // 2:1, 8ms, 150ms, soft, 4dB cap
        case InstrumentType::ElectricGuitar:
            return { true, 2.0f, 5.0f, 150.0f, 2.0f, 4.0f };  // 2:1, 5ms, 150ms, med, 4dB cap
        case InstrumentType::Piano:
            return { true, 1.5f, 15.0f, 250.0f, 4.0f, 3.0f }; // 1.5:1, 15ms, 250ms, soft, 3dB cap
        case InstrumentType::Keys:
            return { true, 1.5f, 15.0f, 250.0f, 4.0f, 3.0f }; // 1.5:1, 15ms, 250ms, soft, 3dB cap
        case InstrumentType::Pad:
            return { false, 1.0f, 15.0f, 250.0f, 4.0f, 0.0f }; // Pad OFF
        case InstrumentType::Synth:
            return { true, 2.0f, 10.0f, 180.0f, 4.0f, 4.0f }; // 2:1, 10ms, 180ms, soft, 4dB cap
        case InstrumentType::Tracks:
            return { true, 2.0f, 10.0f, 200.0f, 4.0f, 3.0f }; // 2:1, 10ms, 200ms, soft, 3dB cap
        case InstrumentType::Room:
            return { false, 1.0f, 10.0f, 200.0f, 4.0f, 0.0f }; // Room OFF
        case InstrumentType::Click:
            return { false, 1.0f, 10.0f, 200.0f, 4.0f, 0.0f }; // Click OFF
        case InstrumentType::Unknown:
        default:
            return { true, 2.0f, 10.0f, 200.0f, 4.0f, 3.0f }; // 2:1, 10ms, 200ms, soft, 3dB cap
    }
}

SafeChain::SafeChain() {
    smoothedTotalGain.setCurrentAndTargetValue(1.0f);
}

SafeChain::~SafeChain() = default;

void SafeChain::prepare(const juce::dsp::ProcessSpec& spec) {
    currentSampleRate = (spec.sampleRate > 0.0) ? spec.sampleRate : 48000.0;

    // 200 ms gain smoothing on audio thread
    smoothedTotalGain.reset(currentSampleRate, 0.200);
    smoothedTotalGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(getTotalTrimDb()));

    // Compressor time constants: 10ms attack, 150ms release
    compAttackCoeff = std::exp(-1.0f / (0.010f * static_cast<float>(currentSampleRate)));
    compReleaseCoeff = std::exp(-1.0f / (0.150f * static_cast<float>(currentSampleRate)));

    updateFilterCoefficients(targetHpfHz.load(), targetLpfHz.load());
    reset();
}

void SafeChain::reset() {
    hpfLeft.reset();
    hpfRight.reset();
    lpfLeft.reset();
    lpfRight.reset();
    compEnvelope = 1.0f;
    currentCompGrDb.store(0.0f, std::memory_order_relaxed);
}

void SafeChain::setCompAttackMs(float ms) noexcept {
    currentCompAttackMs.store(ms, std::memory_order_relaxed);
    if (currentSampleRate > 0.0) {
        compAttackCoeff = std::exp(-1.0f / (std::max(0.0005f, ms * 0.001f) * static_cast<float>(currentSampleRate)));
    }
}

void SafeChain::setCompReleaseMs(float ms) noexcept {
    currentCompReleaseMs.store(ms, std::memory_order_relaxed);
    if (currentSampleRate > 0.0) {
        compReleaseCoeff = std::exp(-1.0f / (std::max(0.001f, ms * 0.001f) * static_cast<float>(currentSampleRate)));
    }
}

void SafeChain::resetCompToAuto() {
    compManualOverride.store(false, std::memory_order_relaxed);
    auto compCfg = getCompSettingsForInstrument(currentInstrument);
    compEnabled.store(compCfg.enabled, std::memory_order_relaxed);
    compRatio.store(compCfg.ratio, std::memory_order_relaxed);
    currentCompKneeDb.store(compCfg.kneeDb, std::memory_order_relaxed);
    currentCompMaxGrCapDb.store(compCfg.maxGrCapDb, std::memory_order_relaxed);
    setCompAttackMs(compCfg.attackMs);
    setCompReleaseMs(compCfg.releaseMs);
}

void SafeChain::setInstrumentType(InstrumentType newType) {
    if (currentInstrument == newType) return;
    currentInstrument = newType;

    auto defaults = getFilterDefaultsForInstrument(newType);

    // Only update HPF/LPF if user has not engaged manual override
    if (!hpfManualOverride.load()) {
        targetHpfHz.store(defaults.hpfHz);
    }
    if (!lpfManualOverride.load()) {
        targetLpfHz.store(defaults.lpfHz);
    }

    // Playbook leveling comp auto-configuration (§1a)
    if (!compManualOverride.load(std::memory_order_relaxed)) {
        auto compCfg = getCompSettingsForInstrument(newType);
        compEnabled.store(compCfg.enabled, std::memory_order_relaxed);
        compRatio.store(compCfg.ratio, std::memory_order_relaxed);
        currentCompKneeDb.store(compCfg.kneeDb, std::memory_order_relaxed);
        currentCompMaxGrCapDb.store(compCfg.maxGrCapDb, std::memory_order_relaxed);
        setCompAttackMs(compCfg.attackMs);
        setCompReleaseMs(compCfg.releaseMs);
    }

    updateFilterCoefficients(targetHpfHz.load(), targetLpfHz.load());
}

void SafeChain::setManualTrimDb(float db) noexcept {
    manualTrimDb.store(db, std::memory_order_relaxed);
    // Pauses auto-trim for 10 seconds when user grabs manual trim
    autoTrimPauseSeconds.store(10.0f, std::memory_order_relaxed);
}

void SafeChain::setHpfFrequency(float hz, bool isUserManual) {
    float clamped = std::clamp(hz, 20.0f, 1000.0f);
    targetHpfHz.store(clamped);
    if (isUserManual) {
        hpfManualOverride.store(true);
    }
    updateFilterCoefficients(targetHpfHz.load(), targetLpfHz.load());
}

void SafeChain::setLpfFrequency(float hz, bool isUserManual) {
    float clamped = std::clamp(hz, 1000.0f, 20000.0f);
    targetLpfHz.store(clamped);
    if (isUserManual) {
        lpfManualOverride.store(true);
    }
    updateFilterCoefficients(targetHpfHz.load(), targetLpfHz.load());
}

void SafeChain::resetHpfToAuto() {
    hpfManualOverride.store(false);
    auto defaults = getFilterDefaultsForInstrument(currentInstrument);
    targetHpfHz.store(defaults.hpfHz);
    updateFilterCoefficients(targetHpfHz.load(), targetLpfHz.load());
}

void SafeChain::resetLpfToAuto() {
    lpfManualOverride.store(false);
    auto defaults = getFilterDefaultsForInstrument(currentInstrument);
    targetLpfHz.store(defaults.lpfHz);
    updateFilterCoefficients(targetHpfHz.load(), targetLpfHz.load());
}

void SafeChain::updateFilterCoefficients(float hpfHz, float lpfHz) {
    if (currentSampleRate <= 0.0) return;

    // 2nd-order Butterworth High-Pass (Q = 0.7071)
    float safeHpf = std::clamp(hpfHz, 10.0f, static_cast<float>(currentSampleRate * 0.45));
    auto hpfCoefs = FilterCoefs::makeHighPass(currentSampleRate, safeHpf, 0.7071f);
    hpfLeft.coefficients = hpfCoefs;
    hpfRight.coefficients = hpfCoefs;

    // 2nd-order Butterworth Low-Pass (Q = 0.7071)
    float safeLpf = std::clamp(lpfHz, 500.0f, static_cast<float>(currentSampleRate * 0.48));
    auto lpfCoefs = FilterCoefs::makeLowPass(currentSampleRate, safeLpf, 0.7071f);
    lpfLeft.coefficients = lpfCoefs;
    lpfRight.coefficients = lpfCoefs;
}

void SafeChain::updateAutoTrimWorker(float currentPreTrimRmsDb) {
    // If auto-trim is paused by user manual grab, decrement countdown
    float pause = autoTrimPauseSeconds.load(std::memory_order_relaxed);
    if (pause > 0.0f) {
        autoTrimPauseSeconds.store(std::max(0.0f, pause - 0.5f), std::memory_order_relaxed);
        return;
    }

    if (!autoTrimEnabled.load(std::memory_order_relaxed)) {
        return;
    }

    // Freeze trim on silence / near-silence (<-60 dBFS)
    if (currentPreTrimRmsDb < -60.0f) {
        return;
    }

    // Target is -18.0 dBFS RMS
    float errorDb = -18.0f - currentPreTrimRmsDb;
    float clampedTargetTrim = std::clamp(errorDb, -12.0f, 12.0f);

    // Gentle convergence towards target
    float currentTrim = currentAutoTrimDb.load(std::memory_order_relaxed);
    float newTrim = 0.85f * currentTrim + 0.15f * clampedTargetTrim;
    currentAutoTrimDb.store(newTrim, std::memory_order_relaxed);
}

void SafeChain::process(juce::AudioBuffer<float>& buffer) noexcept {
    // PASS-THROUGH RULE: Click tracks are 100% untouched
    if (currentInstrument == InstrumentType::Click) {
        return;
    }

    if (chainBypassed.load(std::memory_order_relaxed)) {
        return;
    }

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    // Update target gain from combined trims
    float targetTotalGain = juce::Decibels::decibelsToGain(getTotalTrimDb());
    smoothedTotalGain.setTargetValue(targetTotalGain);

    float* leftData = buffer.getWritePointer(0);
    float* rightData = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    const bool compActive = compEnabled.load(std::memory_order_relaxed);
    const float threshDb = compThresholdDb.load(std::memory_order_relaxed);
    const float ratio = std::max(1.0f, compRatio.load(std::memory_order_relaxed));
    const float slope = 1.0f - (1.0f / ratio);
    const float maxGrDb = currentCompMaxGrCapDb.load(std::memory_order_relaxed);
    const float kneeWidth = currentCompKneeDb.load(std::memory_order_relaxed);

    float maxBlockGrDb = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        // Step 1: Input Trim (Auto + Manual combined with smoothing)
        float currentGain = smoothedTotalGain.getNextValue();
        float left = leftData[i] * currentGain;
        float right = rightData ? (rightData[i] * currentGain) : left;

        // Step 2 & 3: 2nd-order Butterworth HPF & LPF
        left = hpfLeft.processSample(left);
        left = lpfLeft.processSample(left);

        if (rightData) {
            right = hpfRight.processSample(right);
            right = lpfRight.processSample(right);
        }

        // Step 4: Leveling Compressor (feed-forward, 0-latency)
        if (compActive && ratio > 1.01f && maxGrDb > 0.01f) {
            float detectorLevel = (rightData != nullptr) ? (0.5f * (std::abs(left) + std::abs(right)))
                                                         : std::abs(left);

            float levelDb = (detectorLevel > 1e-5f) ? juce::Decibels::gainToDecibels(detectorLevel) : -100.0f;
            float targetGrDb = 0.0f;

            if (kneeWidth > 0.1f) {
                if (levelDb > (threshDb + kneeWidth * 0.5f)) {
                    targetGrDb = (levelDb - threshDb) * slope;
                } else if (levelDb > (threshDb - kneeWidth * 0.5f)) {
                    float x = levelDb - threshDb + (kneeWidth * 0.5f);
                    targetGrDb = slope * (x * x) / (2.0f * kneeWidth);
                }
            } else {
                if (levelDb > threshDb) {
                    targetGrDb = (levelDb - threshDb) * slope;
                }
            }

            // Clamp max gain reduction
            targetGrDb = std::clamp(targetGrDb, 0.0f, maxGrDb);
            if (targetGrDb > maxBlockGrDb) maxBlockGrDb = targetGrDb;

            float targetLinearGain = juce::Decibels::decibelsToGain(-targetGrDb);

            // Ballistic filter
            if (targetLinearGain < compEnvelope) {
                compEnvelope = compAttackCoeff * compEnvelope + (1.0f - compAttackCoeff) * targetLinearGain;
            } else {
                compEnvelope = compReleaseCoeff * compEnvelope + (1.0f - compReleaseCoeff) * targetLinearGain;
            }

            left *= compEnvelope;
            if (rightData) right *= compEnvelope;
        }

        leftData[i] = left;
        if (rightData) rightData[i] = right;
    }

    currentCompGrDb.store(maxBlockGrDb, std::memory_order_relaxed);
}

} // namespace aimixer
