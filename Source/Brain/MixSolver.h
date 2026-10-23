#pragma once

#include "../Common/Types.h"
#include "../Common/SharedProtocol.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace aimixer {

struct TargetSolverState {
    float slowSmoothedRms = -100.0f;
    float currentRideDb = 0.0f;
    float currentSolverTrimDb = 0.0f;
    int hotPeakCycles = 0;
    float lowRmsSeconds = 0.0f;
    float silentSeconds = 0.0f;
};

struct PairCollisionState {
    int consecutiveCollisions = 0;
    int consecutiveClears = 0;
    uint64_t holdUntilMs = 0;
    uint64_t dampenUntilMs = 0;
    int directionFlipsIn10s = 0;
    uint64_t lastFlipTimestampMs = 0;
    float lastAppliedCutDb = 0.0f;
};

struct SolverState {
    uint32_t solverSequence = 0;
    bool globalSolverEnabled = true;

    // Arrangement tracking
    int prevActiveCount = 0;
    float prevTotalLevel = -100.0f;
    uint64_t chorusFreezeUntilMs = 0;

    std::unordered_map<uint32_t, TargetSolverState> instanceStates;
    std::unordered_map<uint32_t, PairCollisionState> pairStates; // key: (winnerId << 16) | loserId
};

class MixSolver {
public:
    MixSolver() = default;

    // Pure function core: evaluates registry snapshot and generates directives map
    static std::unordered_map<uint32_t, InstanceDirectives> solve(
        const std::vector<InstanceTelemetry>& snapshot,
        uint64_t nowMs,
        SolverState& state,
        const GlobalTasteState& tasteState = {}
    );

    // Instrument priority lookup
    static uint8_t getInstrumentPriority(InstrumentType type) noexcept;

    // Relative ride target vs reference (dB)
    static float getRelativeRideTarget(InstrumentType type, bool isSpeechMode) noexcept;

    // Macro pure calculation helpers
    static float calculateVocalForwardDelta(InstrumentType type, float macroVocalForward) noexcept;
    static float calculateEnergyMaskingScalar(float macroEnergy, float profileMaskingScale, float followPercent) noexcept;
    static float calculateWarmthScalar(float macroSaturation, float profileWarmthScale, float followPercent) noexcept;

private:
    static bool isBandInPairRegion(size_t bandIdx, InstrumentType winner, InstrumentType loser);
    static float getCutQForFrequency(float freqHz) noexcept;
};

} // namespace aimixer
