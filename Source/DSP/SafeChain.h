#pragma once

#include "../Common/Types.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace aimixer {

struct SafeChainFilterDefaults {
    float hpfHz = 40.0f;
    float lpfHz = 20000.0f; // 20kHz = essentially OFF
    bool compEnabled = true;
};

SafeChainFilterDefaults getFilterDefaultsForInstrument(InstrumentType type) noexcept;

struct CompPlaybookSettings {
    bool enabled = true;
    float ratio = 2.0f;
    float attackMs = 10.0f;
    float releaseMs = 150.0f;
    float kneeDb = 2.0f; // soft = 4.0, med = 2.0, hard = 0.0
    float maxGrCapDb = 4.0f;
};

CompPlaybookSettings getCompSettingsForInstrument(InstrumentType type) noexcept;

class SafeChain {
public:
    SafeChain();
    ~SafeChain();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // ==============================================================================
    // AUDIO THREAD: Zero-allocation, zero-latency DSP processing
    // ==============================================================================
    void process(juce::AudioBuffer<float>& buffer) noexcept;

    // ==============================================================================
    // Parameter & Auto Control (called from worker / message thread)
    // ==============================================================================
    void setInstrumentType(InstrumentType newType);
    void setAutoTrimEnabled(bool enabled) noexcept { autoTrimEnabled.store(enabled, std::memory_order_relaxed); }
    void setManualTrimDb(float db) noexcept;
    void setHpfFrequency(float hz, bool isUserManual = false);
    void setLpfFrequency(float hz, bool isUserManual = false);
    void resetHpfToAuto();
    void resetLpfToAuto();

    void setCompEnabled(bool enabled, bool isUserManual = false) noexcept {
        compEnabled.store(enabled, std::memory_order_relaxed);
        if (isUserManual) compManualOverride.store(true, std::memory_order_relaxed);
    }
    void setCompThresholdDb(float threshDb, bool isUserManual = false) noexcept {
        compThresholdDb.store(threshDb, std::memory_order_relaxed);
        if (isUserManual) compManualOverride.store(true, std::memory_order_relaxed);
    }
    void setCompRatio(float ratio, bool isUserManual = false) noexcept {
        compRatio.store(ratio, std::memory_order_relaxed);
        if (isUserManual) compManualOverride.store(true, std::memory_order_relaxed);
    }
    void setCompAttackMs(float ms) noexcept;
    void setCompReleaseMs(float ms) noexcept;
    void resetCompToAuto();

    void setBypassed(bool bypassed) noexcept { chainBypassed.store(bypassed, std::memory_order_relaxed); }

    // Worker thread feeds current RMS level (at ~2 Hz) to adjust auto-trim
    void updateAutoTrimWorker(float currentPreTrimRmsDb);

    // Atomic meters & status readouts for UI
    float getAutoTrimDb() const noexcept { return currentAutoTrimDb.load(std::memory_order_relaxed); }
    float getManualTrimDb() const noexcept { return manualTrimDb.load(std::memory_order_relaxed); }
    float getTotalTrimDb() const noexcept { return getAutoTrimDb() + getManualTrimDb(); }
    float getHpfHz() const noexcept { return targetHpfHz.load(std::memory_order_relaxed); }
    float getLpfHz() const noexcept { return targetLpfHz.load(std::memory_order_relaxed); }
    float getCompRatio() const noexcept { return compRatio.load(std::memory_order_relaxed); }
    float getCompThresholdDb() const noexcept { return compThresholdDb.load(std::memory_order_relaxed); }
    float getCompAttackMs() const noexcept { return currentCompAttackMs.load(std::memory_order_relaxed); }
    float getCompReleaseMs() const noexcept { return currentCompReleaseMs.load(std::memory_order_relaxed); }
    float getCompMaxGrCapDb() const noexcept { return currentCompMaxGrCapDb.load(std::memory_order_relaxed); }
    float getCompKneeDb() const noexcept { return currentCompKneeDb.load(std::memory_order_relaxed); }
    float getCompGainReductionDb() const noexcept { return currentCompGrDb.load(std::memory_order_relaxed); }
    bool isAutoTrimPaused() const noexcept { return autoTrimPauseSeconds.load(std::memory_order_relaxed) > 0.0f; }
    bool isHpfManual() const noexcept { return hpfManualOverride.load(std::memory_order_relaxed); }
    bool isLpfManual() const noexcept { return lpfManualOverride.load(std::memory_order_relaxed); }
    bool isCompManual() const noexcept { return compManualOverride.load(std::memory_order_relaxed); }
    bool isCompEnabled() const noexcept { return compEnabled.load(std::memory_order_relaxed); }
    bool isAutoTrimEnabled() const noexcept { return autoTrimEnabled.load(std::memory_order_relaxed); }
    bool isAutoTrimAtLimit() const noexcept { return std::abs(currentAutoTrimDb.load(std::memory_order_relaxed)) >= 11.95f; }

private:
    void updateFilterCoefficients(float hpfHz, float lpfHz);

    double currentSampleRate = 48000.0;
    InstrumentType currentInstrument = InstrumentType::Unknown;

    std::atomic<bool> chainBypassed{false};
    std::atomic<bool> autoTrimEnabled{true};
    std::atomic<float> currentAutoTrimDb{0.0f};
    std::atomic<float> manualTrimDb{0.0f};
    std::atomic<float> autoTrimPauseSeconds{0.0f};

    // Filter controls & overrides
    std::atomic<float> targetHpfHz{40.0f};
    std::atomic<float> targetLpfHz{20000.0f};
    std::atomic<bool> hpfManualOverride{false};
    std::atomic<bool> lpfManualOverride{false};

    // Smoothed gain and filter target on audio thread
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedTotalGain;
    float currentSmoothHpf = 40.0f;
    float currentSmoothLpf = 20000.0f;

    // Filters (stereo cascaded 2nd-order Butterworth IIR)
    using Filter = juce::dsp::IIR::Filter<float>;
    using FilterCoefs = juce::dsp::IIR::Coefficients<float>;

    Filter hpfLeft, hpfRight;
    Filter lpfLeft, lpfRight;

    // Compressor parameters & state
    std::atomic<bool> compEnabled{true};
    std::atomic<bool> compManualOverride{false};
    std::atomic<float> compThresholdDb{-18.0f};
    std::atomic<float> compRatio{2.0f};
    std::atomic<float> currentCompAttackMs{10.0f};
    std::atomic<float> currentCompReleaseMs{150.0f};
    std::atomic<float> currentCompKneeDb{2.0f};
    std::atomic<float> currentCompMaxGrCapDb{4.0f};
    std::atomic<float> currentCompGrDb{0.0f};

    float compEnvelope = 0.0f;
    float compAttackCoeff = 0.0f;
    float compReleaseCoeff = 0.0f;
};

} // namespace aimixer
