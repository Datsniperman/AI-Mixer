#include "BusStage.h"

namespace aimixer {

BusGlueConfig BusStage::getConfigForBusType(BusType type) noexcept {
    switch (type) {
        case BusType::Drums:
            // 2:1 / 30ms / 180ms / cap 3dB GR + 30% parallel mix, +0.7 dB @ 8kHz shelf
            return { 2.0f, 30.0f, 180.0f, 3.0f, 0.30f, false, 0.0f, 8000.0f, 0.7f };
        case BusType::Vocals:
            // 2:1 / 15ms / 200ms / cap 3dB + de-esser half depth (-3 dB cap), +0.7 dB @ 10kHz shelf
            return { 2.0f, 15.0f, 200.0f, 3.0f, 1.0f, true, -3.0f, 10000.0f, 0.7f };
        case BusType::Music:
            // 1.5:1 / 40ms / 300ms / cap 2dB, flat
            return { 1.5f, 40.0f, 300.0f, 2.0f, 1.0f, false, 0.0f, 0.0f, 0.0f };
        case BusType::FX:
            // 1.2:1 / 50ms / 400ms / cap 1dB, flat
            return { 1.2f, 50.0f, 400.0f, 1.0f, 1.0f, false, 0.0f, 0.0f, 0.0f };
        case BusType::Generic:
        default:
            // 2:1 / 25ms / 220ms / cap 2.5dB, flat
            return { 2.0f, 25.0f, 220.0f, 2.5f, 1.0f, false, 0.0f, 0.0f, 0.0f };
    }
}

BusStage::BusStage() {
    smoothedTrimGain.setCurrentAndTargetValue(1.0f);
    setBusType(BusType::Generic);
}

BusStage::~BusStage() = default;

void BusStage::prepare(const juce::dsp::ProcessSpec& spec) {
    currentSampleRate = (spec.sampleRate > 0.0) ? spec.sampleRate : 48000.0;

    smoothedTrimGain.reset(currentSampleRate, 0.050); // 50 ms smoothing
    smoothedTrimGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(trimDb.load()));

    // Ballistic filter coefficients
    compAttackCoeff = std::exp(-1.0f / (std::max(0.001f, currentConfig.attackMs * 0.001f) * static_cast<float>(currentSampleRate)));
    compReleaseCoeff = std::exp(-1.0f / (std::max(0.010f, currentConfig.releaseMs * 0.001f) * static_cast<float>(currentSampleRate)));

    // De-Esser coefficients
    deEssAttackCoeff = std::exp(-1.0f / (0.005f * static_cast<float>(currentSampleRate)));
    deEssReleaseCoeff = std::exp(-1.0f / (0.040f * static_cast<float>(currentSampleRate)));

    auto bp = FilterCoefs::makeBandPass(currentSampleRate, 6500.0f, 1.4f);
    deEssDetectBandLeft.coefficients = bp;
    deEssDetectBandRight.coefficients = bp;

    auto cut = FilterCoefs::makePeakFilter(currentSampleRate, 6500.0f, 1.4f, 1.0f);
    deEssCutFilterLeft.coefficients = cut;
    deEssCutFilterRight.coefficients = cut;

    updateShelfFilter();

    // Shared FX engines
    reverbEngine.prepare(spec);
    delayEngine.prepare(spec);
    int maxBlock = std::max(64, static_cast<int>(spec.maximumBlockSize));
    wetRevL.assign(maxBlock, 0.0f);
    wetRevR.assign(maxBlock, 0.0f);
    wetDlyL.assign(maxBlock, 0.0f);
    wetDlyR.assign(maxBlock, 0.0f);

    reset();
}

void BusStage::reset() {
    compEnvelope = 1.0f;
    currentGlueGrDb.store(0.0f, std::memory_order_relaxed);

    deEssEnvelope = 0.0f;
    deEssDetectBandLeft.reset();
    deEssDetectBandRight.reset();
    deEssCutFilterLeft.reset();
    deEssCutFilterRight.reset();

    shelfFilterLeft.reset();
    shelfFilterRight.reset();

    reverbEngine.reset();
    delayEngine.reset();
}

void BusStage::applyTasteDirectives(const GlobalTasteState& taste) {
    reverbEngine.setReverbType(taste.reverbType);
    reverbEngine.setDecaySec(taste.reverbDecaySec);
    reverbEngine.setPredelayMs(taste.reverbPredelayMs);
    reverbEngine.setDamping(taste.reverbDamping);

    delayEngine.setSubdivision(taste.delaySubdivision);
    delayEngine.setFeedback(taste.delayFeedback);
    delayEngine.setPingPong(taste.delayPingPong);
}

void BusStage::updateShelfFilter() {
    if (currentSampleRate <= 0.0) return;

    if (currentConfig.shelfGainDb > 0.05f && currentConfig.shelfFreqHz > 100.0f) {
        float linearGain = juce::Decibels::decibelsToGain(currentConfig.shelfGainDb);
        auto coefs = FilterCoefs::makeHighShelf(currentSampleRate, currentConfig.shelfFreqHz, 0.7071f, linearGain);
        shelfFilterLeft.coefficients = coefs;
        shelfFilterRight.coefficients = coefs;
    } else {
        auto flat = FilterCoefs::makePeakFilter(currentSampleRate, 1000.0f, 1.0f, 1.0f);
        shelfFilterLeft.coefficients = flat;
        shelfFilterRight.coefficients = flat;
    }
}

void BusStage::setBusType(BusType type) {
    currentBusType = type;
    currentConfig = getConfigForBusType(type);
    parallelMix.store(currentConfig.parallelMix, std::memory_order_relaxed);

    if (currentSampleRate > 0.0) {
        compAttackCoeff = std::exp(-1.0f / (std::max(0.001f, currentConfig.attackMs * 0.001f) * static_cast<float>(currentSampleRate)));
        compReleaseCoeff = std::exp(-1.0f / (std::max(0.010f, currentConfig.releaseMs * 0.001f) * static_cast<float>(currentSampleRate)));
        updateShelfFilter();
    }
}

void BusStage::process(juce::AudioBuffer<float>& buffer) noexcept {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    float* leftData = buffer.getWritePointer(0);
    float* rightData = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    if (currentBusType == BusType::FX) {
        // Dedicated FX Return Host Mode: 100% wet reverb & delay summed
        if (static_cast<int>(wetRevL.size()) < numSamples) {
            wetRevL.resize(static_cast<size_t>(numSamples), 0.0f);
            wetRevR.resize(static_cast<size_t>(numSamples), 0.0f);
            wetDlyL.resize(static_cast<size_t>(numSamples), 0.0f);
            wetDlyR.resize(static_cast<size_t>(numSamples), 0.0f);
        }

        reverbEngine.process(leftData, rightData ? rightData : leftData, wetRevL.data(), wetRevR.data(), static_cast<size_t>(numSamples));
        delayEngine.process(leftData, rightData ? rightData : leftData, wetDlyL.data(), wetDlyR.data(), static_cast<size_t>(numSamples));

        for (int i = 0; i < numSamples; ++i) {
            leftData[i] = wetRevL[static_cast<size_t>(i)] + wetDlyL[static_cast<size_t>(i)];
            if (rightData) {
                rightData[i] = wetRevR[static_cast<size_t>(i)] + wetDlyR[static_cast<size_t>(i)];
            }
        }
        return;
    }

    smoothedTrimGain.setTargetValue(juce::Decibels::decibelsToGain(trimDb.load(std::memory_order_relaxed)));

    const bool glueActive = compEnabled.load(std::memory_order_relaxed);
    const float ratio = std::max(1.0f, currentConfig.ratio);
    const float slope = 1.0f - (1.0f / ratio);
    const float maxCap = currentConfig.maxGrCapDb;
    const float threshDb = -18.0f; // Target RMS reference threshold
    const float mix = parallelMix.load(std::memory_order_relaxed);
    const bool hasShelf = (currentConfig.shelfGainDb > 0.05f);
    const bool hasDeEss = currentConfig.hasDeEsser;

    float maxGrDbThisBlock = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        // Step 1: Input Trim
        float gTrim = smoothedTrimGain.getNextValue();
        float inL = leftData[i] * gTrim;
        float inR = rightData ? (rightData[i] * gTrim) : inL;

        float outL = inL;
        float outR = inR;

        // Step 2: Glue Compressor
        if (glueActive && ratio > 1.01f && maxCap > 0.01f) {
            float det = (rightData != nullptr) ? (0.5f * (std::abs(inL) + std::abs(inR))) : std::abs(inL);
            float levelDb = (det > 1e-5f) ? juce::Decibels::gainToDecibels(det) : -100.0f;

            float targetGrDb = 0.0f;
            // 2 dB soft knee
            const float kneeWidth = 2.0f;
            if (levelDb > (threshDb + kneeWidth * 0.5f)) {
                targetGrDb = (levelDb - threshDb) * slope;
            } else if (levelDb > (threshDb - kneeWidth * 0.5f)) {
                float x = levelDb - threshDb + (kneeWidth * 0.5f);
                targetGrDb = slope * (x * x) / (2.0f * kneeWidth);
            }

            targetGrDb = std::clamp(targetGrDb, 0.0f, maxCap);
            if (targetGrDb > maxGrDbThisBlock) maxGrDbThisBlock = targetGrDb;

            float targetLinear = juce::Decibels::decibelsToGain(-targetGrDb);
            if (targetLinear < compEnvelope) {
                compEnvelope = compAttackCoeff * compEnvelope + (1.0f - compAttackCoeff) * targetLinear;
            } else {
                compEnvelope = compReleaseCoeff * compEnvelope + (1.0f - compReleaseCoeff) * targetLinear;
            }

            // Parallel blend: (1 - mix) * dry + mix * comp
            float wetGain = compEnvelope;
            outL = (1.0f - mix) * inL + mix * (inL * wetGain);
            outR = (1.0f - mix) * inR + mix * (inR * wetGain);
        }

        // Step 3: De-Esser (Vocals bus only)
        if (hasDeEss) {
            float sibL = deEssDetectBandLeft.processSample(outL);
            float sibR = rightData ? deEssDetectBandRight.processSample(outR) : sibL;
            float sibE = 0.5f * (std::abs(sibL) + std::abs(sibR));
            float broadE = 0.5f * (std::abs(outL) + std::abs(outR));

            float targetCutDb = 0.0f;
            float sibDb = (sibE > 1e-5f) ? juce::Decibels::gainToDecibels(sibE) : -100.0f;
            const float deEssThreshDb = -24.0f;
            if (sibDb > deEssThreshDb) {
                float excess = sibDb - deEssThreshDb;
                targetCutDb = std::clamp(excess * 0.6f, 0.0f, std::abs(currentConfig.deEsserCapDb));
            }

            if (targetCutDb > deEssEnvelope) {
                deEssEnvelope = deEssAttackCoeff * deEssEnvelope + (1.0f - deEssAttackCoeff) * targetCutDb;
            } else {
                deEssEnvelope = deEssReleaseCoeff * deEssEnvelope + (1.0f - deEssReleaseCoeff) * targetCutDb;
            }

            if (deEssEnvelope > 0.05f) {
                float cutLinear = juce::Decibels::decibelsToGain(-deEssEnvelope);
                auto dyn = FilterCoefs::makePeakFilter(currentSampleRate, 6500.0f, 1.4f, cutLinear);
                deEssCutFilterLeft.coefficients = dyn;
                deEssCutFilterRight.coefficients = dyn;

                outL = deEssCutFilterLeft.processSample(outL);
                if (rightData) outR = deEssCutFilterRight.processSample(outR);
            }
        }

        // Step 4: Character Shelf
        if (hasShelf) {
            outL = shelfFilterLeft.processSample(outL);
            if (rightData) outR = shelfFilterRight.processSample(outR);
        }

        leftData[i] = outL;
        if (rightData) rightData[i] = outR;
    }

    currentGlueGrDb.store(maxGrDbThisBlock, std::memory_order_relaxed);
}

} // namespace aimixer
