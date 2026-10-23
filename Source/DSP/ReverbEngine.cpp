#include "ReverbEngine.h"

namespace aimixer {

static const int DEFAULT_COMB_TUNING_L[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
static const int DEFAULT_COMB_TUNING_R[8] = { 1116+23, 1188+23, 1277+23, 1356+23, 1422+23, 1491+23, 1557+23, 1617+23 };

static const int DEFAULT_ALLPASS_TUNING_L[4] = { 556, 441, 341, 225 };
static const int DEFAULT_ALLPASS_TUNING_R[4] = { 556+23, 441+23, 341+23, 225+23 };

ReverbEngine::ReverbEngine() {
    updateParameters();
}

void ReverbEngine::prepare(const juce::dsp::ProcessSpec& spec) {
    sampleRate = spec.sampleRate > 0 ? spec.sampleRate : 48000.0;
    float rateScale = static_cast<float>(sampleRate / 44100.0);

    // Max 100 ms predelay buffer
    predelaySize = static_cast<int>(sampleRate * 0.100);
    predelayBufferL.assign(static_cast<size_t>(predelaySize), 0.0f);
    predelayBufferR.assign(static_cast<size_t>(predelaySize), 0.0f);
    predelayWriteIdx = 0;

    for (size_t i = 0; i < NUM_COMBS; ++i) {
        int sizeL = std::max(16, static_cast<int>(DEFAULT_COMB_TUNING_L[i] * rateScale));
        int sizeR = std::max(16, static_cast<int>(DEFAULT_COMB_TUNING_R[i] * rateScale));
        combsL[i].setBufferSize(sizeL);
        combsR[i].setBufferSize(sizeR);
    }

    for (size_t i = 0; i < NUM_ALLPASSES; ++i) {
        int sizeL = std::max(8, static_cast<int>(DEFAULT_ALLPASS_TUNING_L[i] * rateScale));
        int sizeR = std::max(8, static_cast<int>(DEFAULT_ALLPASS_TUNING_R[i] * rateScale));
        allpassesL[i].setBufferSize(sizeL);
        allpassesR[i].setBufferSize(sizeR);
    }

    reset();
    updateParameters();
}

void ReverbEngine::reset() {
    tailSamplesRemaining = 0;
    std::fill(predelayBufferL.begin(), predelayBufferL.end(), 0.0f);
    std::fill(predelayBufferR.begin(), predelayBufferR.end(), 0.0f);
    predelayWriteIdx = 0;

    for (size_t i = 0; i < NUM_COMBS; ++i) {
        combsL[i].reset();
        combsR[i].reset();
    }

    for (size_t i = 0; i < NUM_ALLPASSES; ++i) {
        allpassesL[i].reset();
        allpassesR[i].reset();
    }
}

void ReverbEngine::setReverbType(ReverbType type) {
    currentType = type;
    updateParameters();
}

void ReverbEngine::setDecaySec(float decaySec) {
    currentDecaySec = std::clamp(decaySec, 0.4f, 6.0f);
    updateParameters();
}

void ReverbEngine::setPredelayMs(float predelayMs) {
    currentPredelayMs = std::clamp(predelayMs, 0.0f, 50.0f);
    updateParameters();
}

void ReverbEngine::setDamping(float damping) {
    currentDamping = std::clamp(damping, 0.0f, 1.0f);
    updateParameters();
}

void ReverbEngine::updateParameters() {
    if (predelaySize > 0) {
        predelaySamples = std::clamp(static_cast<int>(currentPredelayMs * 0.001f * sampleRate), 0, predelaySize - 1);
    }

    float dampFactor = std::clamp(currentDamping * 0.45f, 0.0f, 0.85f);
    if (currentType == ReverbType::Plate) {
        dampFactor *= 0.7f; // Plate is brighter
    } else if (currentType == ReverbType::Room) {
        dampFactor = std::min(0.85f, dampFactor * 1.3f); // Room has more wall absorption
    }

    // Compensate decay calculation for lowpass damping loop loss
    float effectiveDecay = currentDecaySec * (1.0f + dampFactor * 0.6f);

    float fs = static_cast<float>(sampleRate);
    for (size_t i = 0; i < NUM_COMBS; ++i) {
        float delayL = static_cast<float>(combsL[i].getDelaySamples());
        float delayR = static_cast<float>(combsR[i].getDelaySamples());

        // Exact RT60 formula: feedback = 10^(-3 * D / (T60 * fs))
        float fbL = std::clamp(std::pow(10.0f, -3.0f * delayL / (effectiveDecay * fs)), 0.0f, 0.98f);
        float fbR = std::clamp(std::pow(10.0f, -3.0f * delayR / (effectiveDecay * fs)), 0.0f, 0.98f);

        combsL[i].setFeedback(fbL);
        combsR[i].setFeedback(fbR);

        combsL[i].setDamping(dampFactor);
        combsR[i].setDamping(dampFactor);
    }
}

void ReverbEngine::process(const float* inL, const float* inR, float* outWetL, float* outWetR, size_t numSamples) noexcept {
    if (predelaySize <= 0) {
        if (outWetL) std::fill_n(outWetL, numSamples, 0.0f);
        if (outWetR) std::fill_n(outWetR, numSamples, 0.0f);
        return;
    }

    // Measure input peak in block
    float inPeak = 0.0f;
    for (size_t s = 0; s < numSamples; ++s) {
        float peak = std::max(std::abs(inL[s]), inR ? std::abs(inR[s]) : std::abs(inL[s]));
        if (peak > inPeak) inPeak = peak;
    }

    // Signal-present / tail-active gate:
    // When input is active (> 1e-4), replenish hangover counter to predelay + full RT60 decay.
    // When input is silent, decrement counter until tail is finished, then sleep engine.
    if (inPeak > 1e-4f) {
        tailSamplesRemaining = static_cast<int>((currentPredelayMs * 0.001f + currentDecaySec * 1.5f) * sampleRate);
    } else {
        if (tailSamplesRemaining <= 0) {
            if (outWetL) std::fill_n(outWetL, numSamples, 0.0f);
            if (outWetR) std::fill_n(outWetR, numSamples, 0.0f);
            return;
        }
        tailSamplesRemaining -= static_cast<int>(numSamples);
    }

    for (size_t s = 0; s < numSamples; ++s) {
        // Write to predelay circular buffer
        predelayBufferL[static_cast<size_t>(predelayWriteIdx)] = inL[s];
        predelayBufferR[static_cast<size_t>(predelayWriteIdx)] = inR[s];

        // Read predelayed signal
        int readIdx = predelayWriteIdx - predelaySamples;
        if (readIdx < 0) readIdx += predelaySize;

        float preL = predelayBufferL[static_cast<size_t>(readIdx)];
        float preR = predelayBufferR[static_cast<size_t>(readIdx)];
        float preMono = (preL + preR) * 0.5f;

        if (++predelayWriteIdx >= predelaySize) predelayWriteIdx = 0;

        // Feed 8 parallel combs
        float outL = 0.0f;
        float outR = 0.0f;

        for (size_t c = 0; c < NUM_COMBS; ++c) {
            outL += combsL[c].process(preMono);
            outR += combsR[c].process(preMono);
        }

        // Normalize comb mix (gain scalar 0.125f)
        outL *= 0.125f;
        outR *= 0.125f;

        // Series allpass diffusers
        for (size_t a = 0; a < NUM_ALLPASSES; ++a) {
            outL = allpassesL[a].process(outL);
            outR = allpassesR[a].process(outR);
        }

        outWetL[s] = outL;
        outWetR[s] = outR;
    }
}

} // namespace aimixer
