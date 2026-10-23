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

// ==============================================================================
// 1b. Gate / Expander Configuration & State
// ==============================================================================
struct GatePlaybookConfig {
    bool enabled = false;
    bool isGate = false; // true = gate (drums), false = downward expander (voices, guitars)
    float thresholdOffsetDb = 12.0f; // Offset above 10th percentile noise floor
    float maxRangeDb = -18.0f; // -18 dB for drums, -9 dB for voices/guitars
    float holdMs = 80.0f;
    float releaseMs = 60.0f;
};

// ==============================================================================
// 1c. Character EQ Preset Definition
// ==============================================================================
enum class CharacterFilterType : uint8_t {
    Off = 0,
    Peak,
    LowShelf,
    HighShelf
};

struct CharacterFilterDef {
    CharacterFilterType type = CharacterFilterType::Off;
    float freqHz = 1000.0f;
    float gainDb = 0.0f; // Capped to +/-2.5 dB
    float q = 1.0f;
};

struct CharacterEqPreset {
    CharacterFilterDef filters[3];
};

// ==============================================================================
// 1h. Pan & Width Configuration
// ==============================================================================
struct PanWidthConfig {
    float defaultPan = 0.0f;     // -1.0 to +1.0
    float defaultWidth = 1.0f;   // 0.0 to 2.0
    bool bassMonoBelow120Hz = false;
};

// ==============================================================================
// Playbook Stage Class (Source role DSP chain)
// ==============================================================================
class PlaybookStage {
public:
    PlaybookStage();
    ~PlaybookStage();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // AUDIO THREAD: Zero-allocation, zero-latency DSP processing
    void process(juce::AudioBuffer<float>& buffer) noexcept;

    // Called on identity or side hint change
    void setInstrumentIdentity(InstrumentType type, SideHint side);

    // Taste & Directives Handover
    void applyTasteDirectives(const InstanceDirectives& dir, const GlobalTasteState& taste);
    void setLocalTailEnabled(bool enabled) noexcept { localTailEnabled.store(enabled, std::memory_order_relaxed); }
    bool isLocalTailEnabled() const noexcept { return localTailEnabled.load(std::memory_order_relaxed); }
    void setBpm(double bpm) { delayEngine.setBpm(bpm); }

    // Manual overrides & freeze
    void setFrozen(bool frozen) noexcept { isFrozen.store(frozen, std::memory_order_relaxed); }
    void setManualOverrides(uint32_t flags) noexcept { manualOverrides.store(flags, std::memory_order_relaxed); }

    // Worker thread feeds analysis updates (~2 to 15 Hz)
    void updateWorkerAnalysis(float instantRmsDb, float noiseFloorRmsDb, float spectralCentroidHz);

    // Individual block bypasses
    void setGateBypassed(bool b) noexcept { gateBypassed.store(b, std::memory_order_relaxed); }
    void setEqBypassed(bool b) noexcept { eqBypassed.store(b, std::memory_order_relaxed); }
    void setDeEsserBypassed(bool b) noexcept { deEsserBypassed.store(b, std::memory_order_relaxed); }
    void setWarmthBypassed(bool b) noexcept { warmthBypassed.store(b, std::memory_order_relaxed); }
    void setTransientBypassed(bool b) noexcept { transientBypassed.store(b, std::memory_order_relaxed); }
    void setLocalRiderBypassed(bool b) noexcept { localRiderBypassed.store(b, std::memory_order_relaxed); }
    void setPanWidthBypassed(bool b) noexcept { panWidthBypassed.store(b, std::memory_order_relaxed); }

    // Manual adjustments
    void setManualGateThreshold(float threshDb) noexcept;
    void resetGateToAuto() noexcept;
    void setManualWarmthDrive(float drive) noexcept;
    void resetWarmthToAuto() noexcept;
    void setManualPan(float pan) noexcept;
    void setManualWidth(float width) noexcept;
    void resetPanWidthToAuto() noexcept;

    // UI Readouts
    bool isGateOpen() const noexcept { return gateIsOpen.load(std::memory_order_relaxed); }
    float getGateThresholdDb() const noexcept { return activeGateThresholdDb.load(std::memory_order_relaxed); }
    float getGateGainReductionDb() const noexcept { return currentGateGrDb.load(std::memory_order_relaxed); }
    float getDeEsserGainReductionDb() const noexcept { return currentDeEssGrDb.load(std::memory_order_relaxed); }
    float getWarmthDrive() const noexcept { return activeWarmthDrive.load(std::memory_order_relaxed); }
    float getTransientBoostDb() const noexcept { return activeTransientBoostDb.load(std::memory_order_relaxed); }
    float getLocalRiderGainDb() const noexcept { return currentLocalRideDb.load(std::memory_order_relaxed); }
    float getActivePan() const noexcept { return activePan.load(std::memory_order_relaxed); }
    float getActiveWidth() const noexcept { return activeWidth.load(std::memory_order_relaxed); }
    float getReverbSendDb() const noexcept { return activeReverbSendDb.load(std::memory_order_relaxed); }
    float getDelaySendDb() const noexcept { return activeDelaySendDb.load(std::memory_order_relaxed); }
    float getCharacterTiltDb() const noexcept { return activeCharacterTiltDb.load(std::memory_order_relaxed); }

    // Reverb / Delay engine accessors
    ReverbEngine& getReverbEngine() noexcept { return reverbEngine; }
    DelayEngine& getDelayEngine() noexcept { return delayEngine; }

    // Helpers
    static GatePlaybookConfig getGateConfigForInstrument(InstrumentType type) noexcept;
    static CharacterEqPreset getCharacterEqPreset(InstrumentType type) noexcept;
    static float getSaturationDriveForInstrument(InstrumentType type) noexcept;
    static float getTransientBoostForInstrument(InstrumentType type) noexcept;
    static PanWidthConfig getPanWidthConfig(InstrumentType type, SideHint side) noexcept;

private:
    void updateCharacterFilters();
    void updateDeEsserFilter(float centroidHz);
    void updateBassMonoFilters();
    void updateCharacterTiltFilter();

    double currentSampleRate = 48000.0;
    InstrumentType currentInstrument = InstrumentType::Unknown;
    SideHint currentSide = SideHint::Centre;

    std::atomic<bool> isFrozen{false};
    std::atomic<uint32_t> manualOverrides{ManualOverride::None};

    // Bypasses
    std::atomic<bool> gateBypassed{false};
    std::atomic<bool> eqBypassed{false};
    std::atomic<bool> deEsserBypassed{false};
    std::atomic<bool> warmthBypassed{false};
    std::atomic<bool> transientBypassed{false};
    std::atomic<bool> localRiderBypassed{false};
    std::atomic<bool> panWidthBypassed{false};

    // 1b. Gate / Expander
    GatePlaybookConfig gateConfig;
    std::atomic<bool> gateManualOverride{false};
    std::atomic<float> activeGateThresholdDb{-60.0f};
    std::atomic<bool> gateIsOpen{true};
    std::atomic<float> currentGateGrDb{0.0f};

    float gateSmoothedGain = 1.0f;
    float gateAttackCoeff = 0.0f;
    float gateReleaseCoeff = 0.0f;
    int gateHoldSamplesRemaining = 0;
    int gateMinClosedSamplesRemaining = 0;
    float gateHoldSamplesTotal = 0;
    float gateMinClosedSamplesTotal = 0;
    float lastNoiseFloorRmsDb = -65.0f;
    float targetAutoThresholdDb = -60.0f;

    // 1c. Character EQ
    CharacterEqPreset activeEqPreset;
    using Filter = juce::dsp::IIR::Filter<float>;
    using FilterCoefs = juce::dsp::IIR::Coefficients<float>;

    Filter charFiltersLeft[3];
    Filter charFiltersRight[3];

    // Character Tilt (High Shelf @ 8 kHz, +/-4 dB)
    Filter charTiltFilterLeft, charTiltFilterRight;
    std::atomic<float> activeCharacterTiltDb{0.0f};

    // 1d. De-Esser / Harshness
    bool deEsserEnabled = false;
    float deEsserMaxCapDb = 0.0f;
    float deEsserCenterFreqHz = 6500.0f;
    Filter deEssDetectBandLeft, deEssDetectBandRight;
    Filter deEssCutFilterLeft, deEssCutFilterRight;

    std::atomic<float> currentDeEssGrDb{0.0f};
    float deEssEnvelope = 0.0f;
    float deEssSmoothAttackCoeff = 0.0f;
    float deEssReleaseCoeff = 0.0f;

    // 1e. Saturation / Warmth
    std::atomic<bool> warmthManualOverride{false};
    std::atomic<float> activeWarmthDrive{0.0f};

    // 1f. Transient Stage
    std::atomic<float> activeTransientBoostDb{0.0f};
    float transFastEnvL = 0.0f, transFastEnvR = 0.0f;
    float transSlowEnvL = 0.0f, transSlowEnvR = 0.0f;
    float transFastAttCoeff = 0.0f, transFastRelCoeff = 0.0f;
    float transSlowAttCoeff = 0.0f, transSlowRelCoeff = 0.0f;

    // 1g. Local Rider
    bool localRiderEnabled = false;
    float localRiderWindowMs = 400.0f;
    std::atomic<float> currentLocalRideDb{0.0f};
    float localRmsAccumulator = 0.0f;
    int localRmsSampleCount = 0;
    int localRmsWindowSamples = 19200;
    float localRollingAverageRms = -18.0f;
    float localRiderTargetDb = 0.0f;

    // 1h. Pan & Width + Bass-Mono
    PanWidthConfig panWidthConfig;
    std::atomic<bool> panWidthManualOverride{false};
    std::atomic<float> activePan{0.0f};
    std::atomic<float> activeWidth{1.0f};

    // Linkwitz-Riley 2nd order crossover (LPF & HPF) for bass-mono
    Filter bassMonoLpfLeft, bassMonoLpfRight;
    Filter bassMonoHpfLeft, bassMonoHpfRight;

    // MVP 5 Shared FX (Local Tail Mode)
    ReverbEngine reverbEngine;
    DelayEngine delayEngine;
    std::atomic<bool> localTailEnabled{true};
    std::atomic<float> activeReverbSendDb{-60.0f};
    std::atomic<float> activeDelaySendDb{-60.0f};

    // Real-time scratch buffers for local FX tail mixing (pre-allocated)
    std::vector<float> wetRevL;
    std::vector<float> wetRevR;
    std::vector<float> wetDlyL;
    std::vector<float> wetDlyR;
};

} // namespace aimixer
