#include "LoudnessMeter.h"

namespace aimixer {

LoudnessMeter::LoudnessMeter() {
    prepare(48000.0);
}

LoudnessMeter::~LoudnessMeter() = default;

void LoudnessMeter::prepare(double sr) {
    sampleRate = (sr > 0.0) ? sr : 48000.0;
    momentaryWindowSamples = static_cast<int>(0.400f * static_cast<float>(sampleRate));
    momentaryHopSamples = static_cast<int>(0.100f * static_cast<float>(sampleRate));
    shortTermWindowSamples = static_cast<int>(3.000f * static_cast<float>(sampleRate));

    updateFilters();
    reset();
}

void LoudnessMeter::reset() {
    totalProcessedSeconds = 0.0f;
    stage1Left.reset();
    stage1Right.reset();
    stage2Left.reset();
    stage2Right.reset();

    momentarySquaresLeft.clear();
    momentarySquaresRight.clear();
    momentarySumLeft = 0.0f;
    momentarySumRight = 0.0f;

    shortTermSquares.clear();
    shortTermSum = 0.0f;

    integratedBlocks.clear();
    samplesSinceLastHop = 0;

    currentMomentaryLufs = -100.0f;
    currentShortTermLufs = -100.0f;
    currentIntegratedLufs = -100.0f;
}

void LoudnessMeter::updateFilters() {
    if (sampleRate <= 0.0) return;

    // ITU-R BS.1770-5 Exact Coefficients at 48 kHz
    if (std::abs(sampleRate - 48000.0) < 1.0) {
        // Stage 1: High shelf
        auto s1 = new FilterCoefs(
            1.53512485958697f, -2.69169618940638f, 1.19839281085285f,
            1.0f, -1.69065929318241f, 0.73248077421585f
        );
        stage1Left.coefficients = s1;
        stage1Right.coefficients = s1;

        // Stage 2: High pass
        auto s2 = new FilterCoefs(
            1.0f, -2.0f, 1.0f,
            1.0f, -1.99004745483398f, 0.99007225035649f
        );
        stage2Left.coefficients = s2;
        stage2Right.coefficients = s2;
    } else {
        // General sample rates
        auto s1 = FilterCoefs::makeHighShelf(sampleRate, 1682.0f, 0.7071f, juce::Decibels::decibelsToGain(4.0f));
        stage1Left.coefficients = s1;
        stage1Right.coefficients = s1;

        auto s2 = FilterCoefs::makeHighPass(sampleRate, 38.1f, 0.5003f);
        stage2Left.coefficients = s2;
        stage2Right.coefficients = s2;
    }
}

void LoudnessMeter::processSamples(const float* left, const float* right, int numSamples) noexcept {
    if (numSamples <= 0 || left == nullptr) return;

    totalProcessedSeconds += static_cast<float>(numSamples) / static_cast<float>(sampleRate);

    for (int i = 0; i < numSamples; ++i) {
        float l = left[i];
        float r = right ? right[i] : l;

        // Apply K-weighting Stage 1 & 2
        l = stage1Left.processSample(l);
        l = stage2Left.processSample(l);

        if (right) {
            r = stage1Right.processSample(r);
            r = stage2Right.processSample(r);
        }

        float sqL = l * l;
        float sqR = right ? (r * r) : 0.0f;

        // 400ms sliding window for Momentary
        momentarySquaresLeft.push_back(sqL);
        momentarySquaresRight.push_back(sqR);
        momentarySumLeft += sqL;
        momentarySumRight += sqR;

        if (static_cast<int>(momentarySquaresLeft.size()) > momentaryWindowSamples) {
            momentarySumLeft -= momentarySquaresLeft.front();
            momentarySumRight -= momentarySquaresRight.front();
            momentarySquaresLeft.pop_front();
            momentarySquaresRight.pop_front();
        }

        // 3s sliding window for Short-term
        float sumLR = right ? (sqL + sqR) : sqL;
        shortTermSquares.push_back(sumLR);
        shortTermSum += sumLR;
        if (static_cast<int>(shortTermSquares.size()) > shortTermWindowSamples) {
            shortTermSum -= shortTermSquares.front();
            shortTermSquares.pop_front();
        }

        samplesSinceLastHop++;
        if (samplesSinceLastHop >= momentaryHopSamples) {
            samplesSinceLastHop = 0;

            // Compute current Momentary LUFS
            int mCount = std::max(1, static_cast<int>(momentarySquaresLeft.size()));
            float meanL = momentarySumLeft / static_cast<float>(mCount);
            float meanR = momentarySumRight / static_cast<float>(mCount);
            float power = right ? (meanL + meanR) : meanL;

            float mLufs = (power > 1e-12f) ? (-0.691f + 10.0f * std::log10(power)) : -100.0f;
            currentMomentaryLufs = mLufs;

            // Compute current Short-term LUFS
            int sCount = std::max(1, static_cast<int>(shortTermSquares.size()));
            float sMean = shortTermSum / static_cast<float>(sCount);
            currentShortTermLufs = (sMean > 1e-12f) ? (-0.691f + 10.0f * std::log10(sMean)) : -100.0f;

            // Only accumulate integrated blocks once 400ms buffer is filled
            if (static_cast<int>(momentarySquaresLeft.size()) >= momentaryWindowSamples) {
                integratedBlocks.push_back({ power, mLufs });

                // Periodic calculation of Gated Integrated LUFS
                float absSum = 0.0f;
                int absCount = 0;
                for (const auto& b : integratedBlocks) {
                    if (b.lufs > -70.0f) { // Absolute threshold: -70 LKFS
                        absSum += b.power;
                        absCount++;
                    }
                }

                if (absCount > 0) {
                    float ungatedMeanPower = absSum / static_cast<float>(absCount);
                    float ungatedLufs = -0.691f + 10.0f * std::log10(ungatedMeanPower);
                    float relativeThreshold = ungatedLufs - 10.0f; // -10 LU below ungated mean

                    float gatedSum = 0.0f;
                    int gatedCount = 0;
                    for (const auto& b : integratedBlocks) {
                        if (b.lufs > -70.0f && b.lufs > relativeThreshold) {
                            gatedSum += b.power;
                            gatedCount++;
                        }
                    }

                    if (gatedCount > 0) {
                        float gatedMean = gatedSum / static_cast<float>(gatedCount);
                        currentIntegratedLufs = -0.691f + 10.0f * std::log10(gatedMean);
                    }
                }
            }
        }
    }
}

} // namespace aimixer
