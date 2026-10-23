#include "MasterChain.h"

namespace aimixer {

DestinationSettings MasterChain::getSettingsForDestination(DestinationTag dest) noexcept {
    switch (dest) {
        case DestinationTag::Broadcast:
            return { -14.0f, -1.0f, false }; // -14 LUFS integrated, -1.0 dBTP ceiling
        case DestinationTag::Studio:
            return { -14.0f, -0.3f, false }; // -14 LUFS (studio ref), -0.3 dBTP ceiling
        case DestinationTag::PALive:
        default:
            return { -16.0f, -1.0f, true };  // -16 LUFS integrated, -1.0 dBTP ceiling, slight presence forward
    }
}

MasterChain::MasterChain() {
    smoothedMasterGain.setCurrentAndTargetValue(1.0f);
    destSettings = getSettingsForDestination(DestinationTag::PALive);
}

MasterChain::~MasterChain() = default;

void MasterChain::prepare(const juce::dsp::ProcessSpec& spec) {
    currentSampleRate = (spec.sampleRate > 0.0) ? spec.sampleRate : 48000.0;
    lastMaxBlockSize = std::max(64, static_cast<int>(spec.maximumBlockSize));
    currentNumChannels = std::max(size_t(1), static_cast<size_t>(spec.numChannels));

    // Lookahead: exactly 1.0 ms = round(0.001 * sampleRate) (48 samples @ 48 kHz)
    lookaheadSamples = std::max(1, static_cast<int>(std::round(0.001 * currentSampleRate)));

    // 4x Oversampling (2 stages of 2x)
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        currentNumChannels, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true, false
    );
    oversampler->initProcessing(static_cast<size_t>(lastMaxBlockSize));

    int osDelay = static_cast<int>(std::round(oversampler->getLatencyInSamples()));
    totalReportedLatency = lookaheadSamples + osDelay;

    // Allocate lookahead delay buffer
    delayBuffer.setSize(static_cast<int>(currentNumChannels), lookaheadSamples + 1);
    delayBuffer.clear();
    delayWritePos = 0;

    // Glue Comp ballistics: 40ms attack, 300ms release
    glueAttackCoeff = std::exp(-1.0f / (0.040f * static_cast<float>(currentSampleRate)));
    glueReleaseCoeff = std::exp(-1.0f / (0.300f * static_cast<float>(currentSampleRate)));

    // Limiter ballistics at oversampled rate (4x): 80ms release
    double osRate = currentSampleRate * 4.0;
    limiterReleaseCoeff = std::exp(-1.0f / (0.080f * static_cast<float>(osRate)));

    // Smoothed master gain
    smoothedMasterGain.reset(currentSampleRate, 0.100); // 100 ms smoothing
    smoothedMasterGain.setCurrentAndTargetValue(1.0f);

    updateTonalFilters();
    reset();
}

void MasterChain::reset() {
    if (oversampler) oversampler->reset();
    delayBuffer.clear();
    delayWritePos = 0;

    tonalLowShelfLeft.reset();
    tonalLowShelfRight.reset();
    tonalPresenceLeft.reset();
    tonalPresenceRight.reset();
    tonalAirShelfLeft.reset();
    tonalAirShelfRight.reset();

    glueCompEnvelope = 1.0f;
    currentGlueGrDb.store(0.0f, std::memory_order_relaxed);

    limiterEnvelope = 1.0f;
    currentLimiterGrDb.store(0.0f, std::memory_order_relaxed);

    timeActiveSeconds = 0.0f;
    narrowbandWarningCounter = 0;
    narrowbandWarning.store(false, std::memory_order_relaxed);
}

void MasterChain::setDestination(DestinationTag dest) {
    currentDestination = dest;
    destSettings = getSettingsForDestination(dest);
    updateTonalFilters();
}

void MasterChain::setTasteTonalDelta(float lowDb, float presDb, float airDb) noexcept {
    tasteLowShelfDb.store(lowDb, std::memory_order_relaxed);
    tastePresenceDb.store(presDb, std::memory_order_relaxed);
    tasteAirShelfDb.store(airDb, std::memory_order_relaxed);
    updateTonalFilters();
}

void MasterChain::updateTonalFilters() {
    if (currentSampleRate <= 0.0) return;

    // Filter 1: Low Shelf at 200 Hz
    float lowGain = std::abs(smoothLowShelfDb) > 1.0f ? std::clamp(smoothLowShelfDb, -2.0f, 2.0f) : 0.0f;
    lowGain = std::clamp(lowGain + tasteLowShelfDb.load(std::memory_order_relaxed), -4.0f, 4.0f);
    auto lsCoefs = FilterCoefs::makeLowShelf(currentSampleRate, 200.0f, 0.7071f, juce::Decibels::decibelsToGain(lowGain));
    tonalLowShelfLeft.coefficients = lsCoefs;
    tonalLowShelfRight.coefficients = lsCoefs;

    // Filter 2: Presence Peak at 3000 Hz
    float presGain = std::abs(smoothPresenceDb) > 1.0f ? std::clamp(smoothPresenceDb, -2.0f, 2.0f) : 0.0f;
    if (destSettings.isPresenceForward && std::abs(presGain) < 0.1f) {
        presGain = 0.8f; // PA Live slight presence forward
    }
    presGain = std::clamp(presGain + tastePresenceDb.load(std::memory_order_relaxed), -4.0f, 4.0f);
    auto presCoefs = FilterCoefs::makePeakFilter(currentSampleRate, 3000.0f, 1.0f, juce::Decibels::decibelsToGain(presGain));
    tonalPresenceLeft.coefficients = presCoefs;
    tonalPresenceRight.coefficients = presCoefs;

    // Filter 3: Air Shelf at 10000 Hz
    float airGain = std::abs(smoothAirShelfDb) > 1.0f ? std::clamp(smoothAirShelfDb, -2.0f, 2.0f) : 0.0f;
    airGain = std::clamp(airGain + tasteAirShelfDb.load(std::memory_order_relaxed), -4.0f, 4.0f);
    auto airCoefs = FilterCoefs::makeHighShelf(currentSampleRate, 10000.0f, 0.7071f, juce::Decibels::decibelsToGain(airGain));
    tonalAirShelfLeft.coefficients = airCoefs;
    tonalAirShelfRight.coefficients = airCoefs;
}

void MasterChain::updateWorkerLoudness(float integratedLufs, float /*shortTermLufs*/, float instantRmsDb, const float* spectrum32Bands) {
    if (instantRmsDb > -50.0f) {
        timeActiveSeconds += 0.2f;
    }

    // 3c. Loudness Auto Gain (after >= 10s non-silent program)
    if (autoGainEnabled.load(std::memory_order_relaxed) && timeActiveSeconds >= 10.0f && integratedLufs > -50.0f) {
        float errorDb = destSettings.targetIntegratedLufs - integratedLufs;
        float clampedTarget = std::clamp(errorDb, -4.0f, 4.0f);

        // Slow trim rate <= 0.25 dB/sec (0.05 dB per 200ms worker cycle)
        float currentGain = currentAutoGainDb.load(std::memory_order_relaxed);
        float step = std::clamp(clampedTarget - currentGain, -0.05f, 0.05f);
        currentAutoGainDb.store(currentGain + step, std::memory_order_relaxed);
    }

    // 3d. Tonal Balance EQ (glacial drift toward destination curve over ~30s)
    if (tonalBalanceEnabled.load(std::memory_order_relaxed) && spectrum32Bands != nullptr && timeActiveSeconds >= 5.0f) {
        // Evaluate low band (band 6-8: 80-160 Hz), air (band 25-28: 8-14 kHz)
        float lowEnergy = 0.5f * (spectrum32Bands[6] + spectrum32Bands[7]);
        float airEnergy = 0.5f * (spectrum32Bands[26] + spectrum32Bands[27]);

        // Target: neutral slope (low ~0.55, mid ~0.45, air ~0.35 norm)
        float lowDiff = (0.50f - lowEnergy) * 6.0f;
        float airDiff = (0.35f - airEnergy) * 6.0f;

        targetLowShelfDb = std::clamp(lowDiff, -2.0f, 2.0f);
        targetAirShelfDb = std::clamp(airDiff, -2.0f, 2.0f);

        // 30s glacial drift (~0.015 dB per cycle)
        smoothLowShelfDb = 0.99f * smoothLowShelfDb + 0.01f * targetLowShelfDb;
        smoothAirShelfDb = 0.99f * smoothAirShelfDb + 0.01f * targetAirShelfDb;

        updateTonalFilters();
        currentTonalCorrectionDb.store(std::abs(smoothAirShelfDb) > 0.5f ? smoothAirShelfDb : smoothLowShelfDb, std::memory_order_relaxed);

        // 3g. Sustained narrowband buildup check (feedback hazard)
        bool hasNarrowSpike = false;
        for (size_t b = 2; b < NUM_SPECTRUM_BANDS - 2; ++b) {
            float val = spectrum32Bands[b];
            float neighbors = 0.5f * (spectrum32Bands[b - 1] + spectrum32Bands[b + 1]);
            if (val > 0.70f && (val - neighbors) > 0.35f) {
                hasNarrowSpike = true;
                break;
            }
        }
        if (hasNarrowSpike) {
            narrowbandWarningCounter++;
            if (narrowbandWarningCounter >= 10) { // sustained for 2 seconds
                narrowbandWarning.store(true, std::memory_order_relaxed);
            }
        } else {
            narrowbandWarningCounter = std::max(0, narrowbandWarningCounter - 1);
            if (narrowbandWarningCounter == 0) {
                narrowbandWarning.store(false, std::memory_order_relaxed);
            }
        }
    }
}

juce::String MasterChain::getTonalStatusDescription() const {
    if (!tonalBalanceEnabled.load(std::memory_order_relaxed)) return "Tonal EQ: OFF";
    if (std::abs(smoothAirShelfDb) <= 1.0f && std::abs(smoothLowShelfDb) <= 1.0f) {
        return "Tonal Balance: In Compliance (Transparent)";
    }
    if (smoothAirShelfDb > 1.0f) {
        return "Air +" + juce::String(smoothAirShelfDb, 1) + " dB — mix is darker than " + getDestinationName(currentDestination);
    }
    if (smoothAirShelfDb < -1.0f) {
        return "Air " + juce::String(smoothAirShelfDb, 1) + " dB — mix is brighter than " + getDestinationName(currentDestination);
    }
    if (smoothLowShelfDb > 1.0f) {
        return "Low +" + juce::String(smoothLowShelfDb, 1) + " dB — mix is lean vs " + getDestinationName(currentDestination);
    }
    return "Low " + juce::String(smoothLowShelfDb, 1) + " dB — mix is heavy vs " + getDestinationName(currentDestination);
}

void MasterChain::process(juce::AudioBuffer<float>& buffer) noexcept {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    float* leftData = buffer.getWritePointer(0);
    float* rightData = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    // ----------------------------------------------------------------------
    // Step 1: Tonal Balance EQ
    // ----------------------------------------------------------------------
    if (tonalBalanceEnabled.load(std::memory_order_relaxed)) {
        for (int i = 0; i < numSamples; ++i) {
            float l = leftData[i];
            float r = rightData ? rightData[i] : l;

            l = tonalLowShelfLeft.processSample(l);
            l = tonalPresenceLeft.processSample(l);
            l = tonalAirShelfLeft.processSample(l);

            if (rightData) {
                r = tonalLowShelfRight.processSample(r);
                r = tonalPresenceRight.processSample(r);
                r = tonalAirShelfRight.processSample(r);
                rightData[i] = r;
            }
            leftData[i] = l;
        }
    }

    // ----------------------------------------------------------------------
    // Step 2: Master Glue Comp (1.5:1, 40ms att, 300ms rel, soft knee, 2dB cap)
    // ----------------------------------------------------------------------
    const float glueRatio = 1.5f;
    const float glueSlope = 1.0f - (1.0f / glueRatio);
    const float glueThreshDb = -18.0f;
    const float glueMaxGrDb = 2.0f;
    float maxGlueGrBlock = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        float inL = leftData[i];
        float inR = rightData ? rightData[i] : inL;
        float det = (rightData != nullptr) ? (0.5f * (std::abs(inL) + std::abs(inR))) : std::abs(inL);
        float detDb = (det > 1e-5f) ? juce::Decibels::gainToDecibels(det) : -100.0f;

        float targetGrDb = 0.0f;
        const float knee = 2.0f;
        if (detDb > (glueThreshDb + knee * 0.5f)) {
            targetGrDb = (detDb - glueThreshDb) * glueSlope;
        } else if (detDb > (glueThreshDb - knee * 0.5f)) {
            float x = detDb - glueThreshDb + (knee * 0.5f);
            targetGrDb = glueSlope * (x * x) / (2.0f * knee);
        }
        targetGrDb = std::clamp(targetGrDb, 0.0f, glueMaxGrDb);
        if (targetGrDb > maxGlueGrBlock) maxGlueGrBlock = targetGrDb;

        float targetLin = juce::Decibels::decibelsToGain(-targetGrDb);
        if (targetLin < glueCompEnvelope) {
            glueCompEnvelope = glueAttackCoeff * glueCompEnvelope + (1.0f - glueAttackCoeff) * targetLin;
        } else {
            glueCompEnvelope = glueReleaseCoeff * glueCompEnvelope + (1.0f - glueReleaseCoeff) * targetLin;
        }

        leftData[i] = inL * glueCompEnvelope;
        if (rightData) rightData[i] = inR * glueCompEnvelope;
    }
    currentGlueGrDb.store(maxGlueGrBlock, std::memory_order_relaxed);

    // ----------------------------------------------------------------------
    // Step 3: Loudness Auto Gain
    // ----------------------------------------------------------------------
    float targetGainDb = currentAutoGainDb.load(std::memory_order_relaxed) + manualGainOffsetDb.load(std::memory_order_relaxed);
    smoothedMasterGain.setTargetValue(juce::Decibels::decibelsToGain(targetGainDb));

    for (int i = 0; i < numSamples; ++i) {
        float g = smoothedMasterGain.getNextValue();
        leftData[i] *= g;
        if (rightData) rightData[i] *= g;
    }

    // ----------------------------------------------------------------------
    // Step 4: 4x Oversampled Lookahead Peak Limiter
    // ----------------------------------------------------------------------
    const float ceilingLinear = juce::Decibels::decibelsToGain(destSettings.ceilingDb);
    float maxLimiterGrBlock = 0.0f;

    // Dynamically ensure oversampler matches buffer channel count (handles mono buffers safely)
    if (!oversampler || currentNumChannels != static_cast<size_t>(numChannels)) {
        currentNumChannels = static_cast<size_t>(numChannels);
        oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
            currentNumChannels, 2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true, false
        );
        oversampler->initProcessing(static_cast<size_t>(std::max(64, lastMaxBlockSize)));
        int osDelay = static_cast<int>(std::round(oversampler->getLatencyInSamples()));
        totalReportedLatency = lookaheadSamples + osDelay;
    }

    if (delayBuffer.getNumChannels() != numChannels) {
        delayBuffer.setSize(numChannels, lookaheadSamples + 1);
        delayBuffer.clear();
        delayWritePos = 0;
    }

    // First, pass through 4x oversampler to detect true peaks
    juce::dsp::AudioBlock<float> block(buffer);
    auto osBlock = oversampler->processSamplesUp(block);

    const int osNumSamples = static_cast<int>(osBlock.getNumSamples());
    float* osLeft = osBlock.getChannelPointer(0);
    float* osRight = (osBlock.getNumChannels() > 1) ? osBlock.getChannelPointer(1) : nullptr;

    for (int i = 0; i < osNumSamples; ++i) {
        float peak = std::max(std::abs(osLeft[i]), osRight ? std::abs(osRight[i]) : 0.0f);

        float targetLimiterGain = 1.0f;
        if (peak > ceilingLinear) {
            targetLimiterGain = ceilingLinear / peak;
        }

        // Fast attack (instantaneous peak catch), smooth 80ms release
        if (targetLimiterGain < limiterEnvelope) {
            limiterEnvelope = targetLimiterGain;
        } else {
            limiterEnvelope = limiterReleaseCoeff * limiterEnvelope + (1.0f - limiterReleaseCoeff) * targetLimiterGain;
        }

        float grDb = -juce::Decibels::gainToDecibels(std::clamp(limiterEnvelope, 1e-4f, 1.0f));
        if (grDb > maxLimiterGrBlock) maxLimiterGrBlock = grDb;

        osLeft[i] *= limiterEnvelope;
        if (osRight) osRight[i] *= limiterEnvelope;
    }

    oversampler->processSamplesDown(block);

    // Lookahead delay matching: write into circular buffer and read out delayed
    float* delayL = delayBuffer.getWritePointer(0);
    float* delayR = (delayBuffer.getNumChannels() > 1) ? delayBuffer.getWritePointer(1) : nullptr;
    const int delayCapacity = delayBuffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i) {
        float inL = leftData[i];
        float inR = rightData ? rightData[i] : inL;

        delayL[delayWritePos] = inL;
        if (delayR) delayR[delayWritePos] = inR;

        int readPos = (delayWritePos - lookaheadSamples + delayCapacity) % delayCapacity;
        leftData[i] = delayL[readPos];
        if (rightData && delayR) rightData[i] = delayR[readPos];

        delayWritePos = (delayWritePos + 1) % delayCapacity;
    }

    currentLimiterGrDb.store(maxLimiterGrBlock, std::memory_order_relaxed);
}

} // namespace aimixer
