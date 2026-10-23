#pragma once

#include "../Common/Types.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace aimixer {

class CombFilter {
public:
    void setBufferSize(int size) {
        buffer.assign(static_cast<size_t>(size), 0.0f);
        bufferSize = size;
        bufIdx = 0;
        filterState = 0.0f;
    }

    void setFeedback(float fb) noexcept { feedback = fb; }
    void setDamping(float damp) noexcept { damping = damp; }

    inline float process(float input) noexcept {
        if (bufferSize <= 0) return input;
        float output = buffer[static_cast<size_t>(bufIdx)];
        filterState = (output * (1.0f - damping)) + (filterState * damping);
        buffer[static_cast<size_t>(bufIdx)] = input + (filterState * feedback);
        if (++bufIdx >= bufferSize) bufIdx = 0;
        return output;
    }

    void reset() noexcept {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        bufIdx = 0;
        filterState = 0.0f;
    }

    int getDelaySamples() const noexcept { return bufferSize; }

private:
    std::vector<float> buffer;
    int bufferSize = 0;
    int bufIdx = 0;
    float feedback = 0.5f;
    float filterState = 0.0f;
    float damping = 0.2f;
};

class AllpassFilter {
public:
    void setBufferSize(int size) {
        buffer.assign(static_cast<size_t>(size), 0.0f);
        bufferSize = size;
        bufIdx = 0;
    }

    inline float process(float input) noexcept {
        if (bufferSize <= 0) return input;
        float bufOut = buffer[static_cast<size_t>(bufIdx)];
        float output = -input + bufOut;
        buffer[static_cast<size_t>(bufIdx)] = input + (bufOut * 0.5f);
        if (++bufIdx >= bufferSize) bufIdx = 0;
        return output;
    }

    void reset() noexcept {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        bufIdx = 0;
    }

private:
    std::vector<float> buffer;
    int bufferSize = 0;
    int bufIdx = 0;
};

class ReverbEngine {
public:
    ReverbEngine();
    ~ReverbEngine() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void setReverbType(ReverbType type);
    void setDecaySec(float decaySec);
    void setPredelayMs(float predelayMs);
    void setDamping(float damping);

    ReverbType getReverbType() const noexcept { return currentType; }
    float getDecaySec() const noexcept { return currentDecaySec; }
    float getPredelayMs() const noexcept { return currentPredelayMs; }
    float getDamping() const noexcept { return currentDamping; }

    // Renders wet tail (left & right). REAL-TIME SAFE: zero latency on dry, no locks/allocs
    void process(const float* inL, const float* inR, float* outWetL, float* outWetR, size_t numSamples) noexcept;

private:
    void updateParameters();

    double sampleRate = 48000.0;
    ReverbType currentType = ReverbType::Hall;
    float currentDecaySec = 3.2f;
    float currentPredelayMs = 20.0f;
    float currentDamping = 0.5f;

    // Predelay circular buffer
    std::vector<float> predelayBufferL;
    std::vector<float> predelayBufferR;
    int predelaySize = 0;
    int predelayWriteIdx = 0;
    int predelaySamples = 0;

    // 8 parallel combs per channel
    static constexpr size_t NUM_COMBS = 8;
    CombFilter combsL[NUM_COMBS];
    CombFilter combsR[NUM_COMBS];

    // 4 series allpasses per channel
    static constexpr size_t NUM_ALLPASSES = 4;
    AllpassFilter allpassesL[NUM_ALLPASSES];
    AllpassFilter allpassesR[NUM_ALLPASSES];

    int tailSamplesRemaining = 0;
};

} // namespace aimixer
