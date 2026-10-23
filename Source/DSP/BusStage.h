#pragma once

#include "../Common/Types.h"
#include "../Common/SharedProtocol.h"
#include "ReverbEngine.h"
#include "DelayEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <vector>

namespace aimixer {

struct BusGlueConfig {
    float ratio = 2.0f;
    float attackMs = 25.0f;
    float releaseMs = 220.0f;
    float maxGrCapDb = 2.5f;
    float parallelMix = 1.0f; // 1.0 = 100% wet, 0.3 = 30% wet blended under 100% dry
    bool hasDeEsser = false;
    float deEsserCapDb = 0.0f;
    float shelfFreqHz = 0.0f;
    float shelfGainDb = 0.0f;
};

class BusStage {
public:
    BusStage();
    ~BusStage();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // AUDIO THREAD: Zero-allocation, zero-latency processing
    void process(juce::AudioBuffer<float>& buffer) noexcept;

    void setBusType(BusType type);
    BusType getBusType() const noexcept { return currentBusType; }

    void applyTasteDirectives(const GlobalTasteState& taste);
    void setBpm(double bpm) { delayEngine.setBpm(bpm); }

    void setTrimDb(float db) noexcept { trimDb.store(db, std::memory_order_relaxed); }
    float getTrimDb() const noexcept { return trimDb.load(std::memory_order_relaxed); }

    void setCompEnabled(bool enabled) noexcept { compEnabled.store(enabled, std::memory_order_relaxed); }
    bool isCompEnabled() const noexcept { return compEnabled.load(std::memory_order_relaxed); }

    void setParallelMix(float mix) noexcept { parallelMix.store(std::clamp(mix, 0.0f, 1.0f), std::memory_order_relaxed); }
    float getParallelMix() const noexcept { return parallelMix.load(std::memory_order_relaxed); }

    float getGlueGainReductionDb() const noexcept { return currentGlueGrDb.load(std::memory_order_relaxed); }

    ReverbEngine& getReverbEngine() noexcept { return reverbEngine; }
    DelayEngine& getDelayEngine() noexcept { return delayEngine; }

    static BusGlueConfig getConfigForBusType(BusType type) noexcept;

private:
    void updateShelfFilter();

    double currentSampleRate = 48000.0;
    BusType currentBusType = BusType::Generic;
    BusGlueConfig currentConfig;

    std::atomic<float> trimDb{0.0f};
    std::atomic<bool> compEnabled{true};
    std::atomic<float> parallelMix{1.0f};
    std::atomic<float> currentGlueGrDb{0.0f};

    // Smoothed trim gain
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedTrimGain;

    // Compressor ballistic filter
    float compEnvelope = 1.0f;
    float compAttackCoeff = 0.0f;
    float compReleaseCoeff = 0.0f;

    // De-Esser (Vocals Bus only, half depth)
    using Filter = juce::dsp::IIR::Filter<float>;
    using FilterCoefs = juce::dsp::IIR::Coefficients<float>;

    Filter deEssDetectBandLeft, deEssDetectBandRight;
    Filter deEssCutFilterLeft, deEssCutFilterRight;
    float deEssEnvelope = 0.0f;
    float deEssAttackCoeff = 0.0f;
    float deEssReleaseCoeff = 0.0f;

    // Character Shelf
    Filter shelfFilterLeft, shelfFilterRight;

    // MVP 5 FX Return Mode (BusType::FX)
    ReverbEngine reverbEngine;
    DelayEngine delayEngine;
    std::vector<float> wetRevL;
    std::vector<float> wetRevR;
    std::vector<float> wetDlyL;
    std::vector<float> wetDlyR;
};

} // namespace aimixer
