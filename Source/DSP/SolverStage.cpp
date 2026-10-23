#include "SolverStage.h"
#include <cmath>
#include <algorithm>

namespace aimixer {

SolverStage::SolverStage() {
    smoothedCombinedGain.setCurrentAndTargetValue(1.0f);
    for (int k = 0; k < 4; ++k) {
        publishedCuts[k].store({ false, 1000.0f, 0.0f, 1.4f, ReasonCode::None, InstrumentType::Unknown });
    }
}

SolverStage::~SolverStage() = default;

void SolverStage::prepare(const juce::dsp::ProcessSpec& spec) {
    currentSampleRate = (spec.sampleRate > 0.0) ? spec.sampleRate : 48000.0;

    smoothedCombinedGain.reset(currentSampleRate, 0.100); // 100 ms smoothing
    smoothedCombinedGain.setCurrentAndTargetValue(1.0f);

    // 150 ms ducking release
    duckReleaseCoeff = std::exp(-1.0f / (0.150f * static_cast<float>(currentSampleRate)));

    reset();
}

void SolverStage::reset() {
    for (int k = 0; k < 4; ++k) {
        cutFiltersLeft[k].reset();
        cutFiltersRight[k].reset();
        currentCuts[k] = { false, 1000.0f, 0.0f, 1.4f, ReasonCode::None, InstrumentType::Unknown };
        publishedCuts[k].store(currentCuts[k]);
    }
    currentDuckEnvelope = 0.0f;
    isDirectiveActive.store(false, std::memory_order_relaxed);
}

void SolverStage::updateFilterCoefficients(int index, float freqHz, float gainDb, float q) {
    if (index < 0 || index >= 4 || currentSampleRate <= 0.0) return;

    float safeFreq = std::clamp(freqHz, 20.0f, static_cast<float>(currentSampleRate * 0.48));
    float safeGain = std::clamp(gainDb, -6.0f, 0.0f); // NEGATIVE ONLY
    float safeQ = std::clamp(q, 0.5f, 5.0f);

    float linearGain = juce::Decibels::decibelsToGain(safeGain);
    auto coefs = FilterCoefs::makePeakFilter(currentSampleRate, safeFreq, safeQ, linearGain);

    cutFiltersLeft[index].coefficients = coefs;
    cutFiltersRight[index].coefficients = coefs;
}

void SolverStage::checkStaleness(uint64_t nowMs) {
    if (currentSessionMode.load(std::memory_order_relaxed) == SessionMode::FreezeAll) {
        return;
    }

    if (isDirectiveActive.load(std::memory_order_relaxed) &&
        lastDirectiveTimestampMs > 0 &&
        nowMs > lastDirectiveTimestampMs &&
        (nowMs - lastDirectiveTimestampMs) > DIRECTIVE_STALE_TIMEOUT_MS) {
        
        isDirectiveActive.store(false, std::memory_order_relaxed);
        currentRideDb.store(0.0f, std::memory_order_relaxed);
        currentSolverTrimDb.store(0.0f, std::memory_order_relaxed);
        currentDuckDb.store(0.0f, std::memory_order_relaxed);

        for (int k = 0; k < 4; ++k) {
            currentCuts[k].enabled = false;
            publishedCuts[k].store(currentCuts[k]);
        }
    }
}

void SolverStage::updateDirectives(const InstanceDirectives& dir, uint64_t nowMs) {
    lastDirectiveTimestampMs = nowMs;
    currentSessionMode.store(dir.sessionMode, std::memory_order_relaxed);

    bool isFreezeAll = (dir.sessionMode == SessionMode::FreezeAll);

    // Staleness check: if directives older than 1000 ms, or solver disabled globally
    bool isStale = !isFreezeAll && (nowMs > dir.directiveTimestampMs && (nowMs - dir.directiveTimestampMs) > DIRECTIVE_STALE_TIMEOUT_MS);
    bool enabled = solverEnabled.load(std::memory_order_relaxed) && dir.solverEnabled && !isStale;

    if (!enabled) {
        if (!isFreezeAll) {
            isDirectiveActive.store(false, std::memory_order_relaxed);
            currentRideDb.store(0.0f, std::memory_order_relaxed);
            currentSolverTrimDb.store(0.0f, std::memory_order_relaxed);
            currentDuckDb.store(0.0f, std::memory_order_relaxed);

            for (int k = 0; k < 4; ++k) {
                currentCuts[k].enabled = false;
                publishedCuts[k].store(currentCuts[k]);
            }
        }
        return;
    }

    isDirectiveActive.store(true, std::memory_order_relaxed);

    bool frozen = isFrozen.load(std::memory_order_relaxed);
    uint32_t overrides = manualOverrides.load(std::memory_order_relaxed);

    // Apply Level / Duck Directives
    if (frozen || (overrides & ManualOverride::LevelRide)) {
        currentRideDb.store(0.0f, std::memory_order_relaxed);
        currentDuckDb.store(0.0f, std::memory_order_relaxed);
    } else {
        currentRideDb.store(std::clamp(dir.rideOffsetDb, -4.0f, 4.0f), std::memory_order_relaxed);
        currentDuckDb.store(std::clamp(dir.duckDb, -3.0f, 0.0f), std::memory_order_relaxed);
    }

    // Apply Solver Staging Trim
    if (frozen || (overrides & ManualOverride::Trim)) {
        currentSolverTrimDb.store(0.0f, std::memory_order_relaxed);
    } else {
        currentSolverTrimDb.store(std::clamp(dir.solverTrimDb, -3.0f, 2.0f), std::memory_order_relaxed);
    }

    // Apply Masking Cut Directives
    if (frozen || (overrides & ManualOverride::EQ)) {
        for (int k = 0; k < 4; ++k) {
            currentCuts[k].enabled = false;
            publishedCuts[k].store(currentCuts[k]);
        }
    } else {
        for (int k = 0; k < 4; ++k) {
            const auto& cut = dir.maskingCuts[k];
            if (cut.enabled && cut.gainDb < -0.1f) {
                float safeGain = std::clamp(cut.gainDb, -6.0f, 0.0f);
                bool needsUpdate = !currentCuts[k].enabled ||
                                   std::abs(currentCuts[k].gainDb - safeGain) > 0.25f ||
                                   std::abs(currentCuts[k].freqHz - cut.freqHz) / currentCuts[k].freqHz > 0.05f;

                if (needsUpdate) {
                    updateFilterCoefficients(k, cut.freqHz, safeGain, cut.q);
                }

                currentCuts[k] = { true, cut.freqHz, safeGain, cut.q, cut.reason, cut.otherInstrument };
            } else {
                currentCuts[k].enabled = false;
            }
            publishedCuts[k].store(currentCuts[k]);
        }
    }
}

void SolverStage::process(juce::AudioBuffer<float>& buffer) noexcept {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    float totalGainDb = currentRideDb.load(std::memory_order_relaxed) +
                        currentSolverTrimDb.load(std::memory_order_relaxed) +
                        currentDuckDb.load(std::memory_order_relaxed);

    float linearGain = juce::Decibels::decibelsToGain(totalGainDb);
    smoothedCombinedGain.setTargetValue(linearGain);

    float* leftData = buffer.getWritePointer(0);
    float* rightData = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i) {
        float left = leftData[i];
        float right = rightData ? rightData[i] : left;

        // Apply active masking peaking cuts
        for (int k = 0; k < 4; ++k) {
            if (currentCuts[k].enabled) {
                left = cutFiltersLeft[k].processSample(left);
                if (rightData) {
                    right = cutFiltersRight[k].processSample(right);
                }
            }
        }

        // Apply smoothed solver gain (ride + staging + duck)
        float g = smoothedCombinedGain.getNextValue();
        left *= g;
        if (rightData) right *= g;

        leftData[i] = left;
        if (rightData) rightData[i] = right;
    }
}

MaskingCutDirective SolverStage::getActiveCut(int index) const noexcept {
    if (index < 0 || index >= 4) return {};
    auto s = publishedCuts[index].load();
    MaskingCutDirective d;
    d.enabled = s.enabled;
    d.freqHz = s.freqHz;
    d.gainDb = s.gainDb;
    d.q = s.q;
    d.reason = s.reason;
    d.otherInstrument = s.otherInstrument;
    return d;
}

} // namespace aimixer
