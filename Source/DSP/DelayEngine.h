#pragma once

#include "../Common/Types.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace aimixer {

class DelayEngine {
public:
    DelayEngine();
    ~DelayEngine() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void setBpm(double bpm);
    void setSubdivision(DelaySubdivision sub);
    void setFeedback(float feedback);
    void setPingPong(bool pingPong);

    double getBpm() const noexcept { return currentBpm; }
    DelaySubdivision getSubdivision() const noexcept { return currentSubdivision; }
    float getFeedback() const noexcept { return currentFeedback; }
    bool isPingPong() const noexcept { return currentPingPong; }

    // Returns current delay time in milliseconds (for tests/UI)
    float getDelayTimeMs() const noexcept;

    // AUDIO THREAD: Zero latency on dry signal, generates wet delay tail
    void process(const float* inL, const float* inR, float* outWetL, float* outWetR, size_t numSamples) noexcept;

private:
    void updateDelaySamples();

    double sampleRate = 48000.0;
    double currentBpm = 100.0;
    DelaySubdivision currentSubdivision = DelaySubdivision::DottedEighth;
    float currentFeedback = 0.35f; // Clamped to 0.0 - 0.60
    bool currentPingPong = true;

    int delaySamples = 0;
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int bufferSize = 0;
    int writeIdxL = 0;
    int writeIdxR = 0;

    // Single-pole damping lowpass filters (4 kHz)
    float filterL = 0.0f;
    float filterR = 0.0f;
    float filterCoeff = 0.5f;

    int tailSamplesRemaining = 0;
};

} // namespace aimixer
