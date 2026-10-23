#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <deque>
#include <cmath>
#include <algorithm>

namespace aimixer {

class LoudnessMeter {
public:
    LoudnessMeter();
    ~LoudnessMeter();

    void prepare(double sampleRate);
    void reset();

    // Process a block of stereo audio samples (can be called on worker thread from ring buffer tap or DSP)
    void processSamples(const float* left, const float* right, int numSamples) noexcept;

    // Readouts
    float getMomentaryLufs() const noexcept { return currentMomentaryLufs; }
    float getShortTermLufs() const noexcept { return currentShortTermLufs; }
    float getIntegratedLufs() const noexcept { return currentIntegratedLufs; }

    bool hasSufficientAudio() const noexcept { return totalProcessedSeconds >= 1.0f; }
    float getTotalProcessedSeconds() const noexcept { return totalProcessedSeconds; }

private:
    void updateFilters();

    double sampleRate = 48000.0;
    float totalProcessedSeconds = 0.0f;

    // BS.1770-5 K-weighting filters (Stage 1: High-shelf, Stage 2: High-pass)
    using Filter = juce::dsp::IIR::Filter<float>;
    using FilterCoefs = juce::dsp::IIR::Coefficients<float>;

    Filter stage1Left, stage1Right; // Pre-filter high-shelf
    Filter stage2Left, stage2Right; // High-pass weighting

    // 400ms sliding window accumulator (for Momentary)
    int momentaryWindowSamples = 19200;
    int momentaryHopSamples = 4800; // 100ms hop (75% overlap)
    std::deque<float> momentarySquaresLeft;
    std::deque<float> momentarySquaresRight;
    float momentarySumLeft = 0.0f;
    float momentarySumRight = 0.0f;

    // 3s sliding window accumulator (for Short-term)
    int shortTermWindowSamples = 144000;
    std::deque<float> shortTermSquares;
    float shortTermSum = 0.0f;

    // Integrated Gating blocks (400ms blocks evaluated every 100ms)
    struct GatedBlock {
        float power = 0.0f;
        float lufs = -100.0f;
    };
    std::vector<GatedBlock> integratedBlocks;
    int samplesSinceLastHop = 0;

    float currentMomentaryLufs = -100.0f;
    float currentShortTermLufs = -100.0f;
    float currentIntegratedLufs = -100.0f;
};

} // namespace aimixer
