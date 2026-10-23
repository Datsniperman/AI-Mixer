#pragma once

#include "../Common/Types.h"
#include "../Common/SharedProtocol.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace aimixer {

class SolverStage {
public:
    SolverStage();
    ~SolverStage();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // AUDIO THREAD: Zero-allocation, zero-latency DSP processing
    void process(juce::AudioBuffer<float>& buffer) noexcept;

    // Directives update (called at ~5-15 Hz from worker/timer thread)
    void updateDirectives(const InstanceDirectives& directives, uint64_t nowMs);
    void checkStaleness(uint64_t nowMs);

    // Manual overrides & freeze
    void setFrozen(bool frozen) noexcept { isFrozen.store(frozen, std::memory_order_relaxed); }
    void setManualOverrides(uint32_t flags) noexcept { manualOverrides.store(flags, std::memory_order_relaxed); }
    void setSolverEnabled(bool enabled) noexcept { solverEnabled.store(enabled, std::memory_order_relaxed); }

    // Readouts for UI
    float getActiveRideDb() const noexcept { return currentRideDb.load(std::memory_order_relaxed); }
    float getActiveDuckDb() const noexcept { return currentDuckDb.load(std::memory_order_relaxed); }
    float getActiveTrimDb() const noexcept { return currentSolverTrimDb.load(std::memory_order_relaxed); }
    bool isSolverActive() const noexcept { return isDirectiveActive.load(std::memory_order_relaxed); }
    MaskingCutDirective getActiveCut(int index) const noexcept;

private:
    void updateFilterCoefficients(int index, float freqHz, float gainDb, float q);

    double currentSampleRate = 48000.0;

    std::atomic<bool> solverEnabled{true};
    std::atomic<bool> isFrozen{false};
    std::atomic<uint32_t> manualOverrides{ManualOverride::None};
    std::atomic<SessionMode> currentSessionMode{SessionMode::Live};

    // Stored target parameters from directives
    std::atomic<bool> isDirectiveActive{false};
    std::atomic<float> currentRideDb{0.0f};
    std::atomic<float> currentSolverTrimDb{0.0f};
    std::atomic<float> currentDuckDb{0.0f};

    uint64_t lastDirectiveTimestampMs = 0;

    // Smoothed combined gain (ride + solverTrim + duck)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedCombinedGain;
    float currentDuckEnvelope = 0.0f;
    float duckReleaseCoeff = 0.0f;

    // 4 Peaking Biquads for Masking Cuts (Stereo Left & Right)
    using Filter = juce::dsp::IIR::Filter<float>;
    using FilterCoefs = juce::dsp::IIR::Coefficients<float>;

    Filter cutFiltersLeft[4];
    Filter cutFiltersRight[4];

    struct FilterState {
        bool enabled = false;
        float freqHz = 1000.0f;
        float gainDb = 0.0f;
        float q = 1.4f;
        ReasonCode reason = ReasonCode::None;
        InstrumentType otherInstrument = InstrumentType::Unknown;
    };

    FilterState currentCuts[4];
    std::atomic<FilterState> publishedCuts[4];
};

} // namespace aimixer
