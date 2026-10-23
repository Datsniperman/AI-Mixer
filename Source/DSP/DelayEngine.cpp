#include "DelayEngine.h"

namespace aimixer {

DelayEngine::DelayEngine() {
    updateDelaySamples();
}

void DelayEngine::prepare(const juce::dsp::ProcessSpec& spec) {
    sampleRate = spec.sampleRate > 0 ? spec.sampleRate : 48000.0;

    // Allocate 3 seconds maximum delay buffer
    bufferSize = static_cast<int>(sampleRate * 3.0);
    delayBufferL.assign(static_cast<size_t>(bufferSize), 0.0f);
    delayBufferR.assign(static_cast<size_t>(bufferSize), 0.0f);
    writeIdxL = 0;
    writeIdxR = 0;

    // 4000 Hz single-pole lowpass filter coefficient
    // alpha = exp(-2 * pi * fc / fs)
    filterCoeff = std::clamp(std::exp(-2.0f * 3.14159265f * 4000.0f / static_cast<float>(sampleRate)), 0.1f, 0.95f);

    reset();
    updateDelaySamples();
}

void DelayEngine::reset() {
    tailSamplesRemaining = 0;
    std::fill(delayBufferL.begin(), delayBufferL.end(), 0.0f);
    std::fill(delayBufferR.begin(), delayBufferR.end(), 0.0f);
    writeIdxL = 0;
    writeIdxR = 0;
    filterL = 0.0f;
    filterR = 0.0f;
}

void DelayEngine::setBpm(double bpm) {
    currentBpm = (bpm > 20.0 && bpm < 300.0) ? bpm : 100.0;
    updateDelaySamples();
}

void DelayEngine::setSubdivision(DelaySubdivision sub) {
    currentSubdivision = sub;
    updateDelaySamples();
}

void DelayEngine::setFeedback(float feedback) {
    // Strictly capped to 0.60 to prevent infinite runaway
    currentFeedback = std::clamp(feedback, 0.0f, 0.60f);
}

void DelayEngine::setPingPong(bool pingPong) {
    currentPingPong = pingPong;
}

float DelayEngine::getDelayTimeMs() const noexcept {
    double quarterSec = 60.0 / currentBpm;
    double factor = 0.75; // Dotted eighth default
    switch (currentSubdivision) {
        case DelaySubdivision::Quarter:      factor = 1.0; break;
        case DelaySubdivision::Eighth:       factor = 0.5; break;
        case DelaySubdivision::DottedEighth:
        default:                             factor = 0.75; break;
    }
    return static_cast<float>(quarterSec * factor * 1000.0);
}

void DelayEngine::updateDelaySamples() {
    if (bufferSize <= 0) return;
    float timeMs = getDelayTimeMs();
    delaySamples = std::clamp(static_cast<int>(timeMs * 0.001f * sampleRate), 1, bufferSize - 1);
}

void DelayEngine::process(const float* inL, const float* inR, float* outWetL, float* outWetR, size_t numSamples) noexcept {
    if (bufferSize <= 0 || delaySamples <= 0) {
        if (outWetL) std::fill_n(outWetL, numSamples, 0.0f);
        if (outWetR) std::fill_n(outWetR, numSamples, 0.0f);
        return;
    }

    // Input peak measure
    float inPeak = 0.0f;
    for (size_t s = 0; s < numSamples; ++s) {
        float peak = std::max(std::abs(inL[s]), inR ? std::abs(inR[s]) : std::abs(inL[s]));
        if (peak > inPeak) inPeak = peak;
    }

    // Signal-present / tail gate
    if (inPeak > 1e-4f) {
        float maxTailSec = (getDelayTimeMs() * 0.001f) * 12.0f;
        tailSamplesRemaining = static_cast<int>(maxTailSec * sampleRate);
    } else {
        if (tailSamplesRemaining <= 0) {
            if (outWetL) std::fill_n(outWetL, numSamples, 0.0f);
            if (outWetR) std::fill_n(outWetR, numSamples, 0.0f);
            return;
        }
        tailSamplesRemaining -= static_cast<int>(numSamples);
    }

    const float oneMinusCoeff = 1.0f - filterCoeff;

    for (size_t s = 0; s < numSamples; ++s) {
        if (currentPingPong) {
            float monoIn = (inL[s] + inR[s]) * 0.5f;

            // Read Left delayed output (taps at T)
            int readIdxL = writeIdxL - delaySamples;
            if (readIdxL < 0) readIdxL += bufferSize;
            float outL = delayBufferL[static_cast<size_t>(readIdxL)];
            filterL = oneMinusCoeff * outL + filterCoeff * filterL;

            // Read Right delayed output (taps at 2T)
            int readIdxR = writeIdxR - delaySamples;
            if (readIdxR < 0) readIdxR += bufferSize;
            float outR = delayBufferR[static_cast<size_t>(readIdxR)];
            filterR = oneMinusCoeff * outR + filterCoeff * filterR;

            // Ping-pong cross feedback:
            // Left gets input + filtered Right feedback
            delayBufferL[static_cast<size_t>(writeIdxL)] = monoIn + (filterR * currentFeedback);

            // Right gets filtered Left output (which tapped at T, so Right will tap at 2T)
            delayBufferR[static_cast<size_t>(writeIdxR)] = filterL;

            if (++writeIdxL >= bufferSize) writeIdxL = 0;
            if (++writeIdxR >= bufferSize) writeIdxR = 0;

            outWetL[s] = outL;
            outWetR[s] = outR;
        } else {
            // Standard stereo delay
            int readIdxL = writeIdxL - delaySamples;
            if (readIdxL < 0) readIdxL += bufferSize;
            float outL = delayBufferL[static_cast<size_t>(readIdxL)];
            filterL = oneMinusCoeff * outL + filterCoeff * filterL;

            int readIdxR = writeIdxR - delaySamples;
            if (readIdxR < 0) readIdxR += bufferSize;
            float outR = delayBufferR[static_cast<size_t>(readIdxR)];
            filterR = oneMinusCoeff * outR + filterCoeff * filterR;

            delayBufferL[static_cast<size_t>(writeIdxL)] = inL[s] + (filterL * currentFeedback);
            delayBufferR[static_cast<size_t>(writeIdxR)] = inR[s] + (filterR * currentFeedback);

            if (++writeIdxL >= bufferSize) writeIdxL = 0;
            if (++writeIdxR >= bufferSize) writeIdxR = 0;

            outWetL[s] = outL;
            outWetR[s] = outR;
        }
    }
}

} // namespace aimixer
