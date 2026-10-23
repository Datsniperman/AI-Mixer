#pragma once

#include "Types.h"
#include <atomic>
#include <cstdint>
#include <cstring>

namespace aimixer {

constexpr uint32_t SHARED_MAGIC = 0x41494D58; // 'AIMX'
constexpr uint32_t PROTOCOL_VERSION = 5;       // Bumped for MVP 5 Taste Profile & Shared FX
constexpr size_t MAX_REGISTRY_INSTANCES = 64;
constexpr size_t NUM_SPECTRUM_BANDS = 32;
constexpr size_t NUM_FX_CATEGORIES = 8;
constexpr size_t NUM_INSTRUMENT_SLOTS = 21;
constexpr uint64_t HEARTBEAT_TIMEOUT_MS = 2500; // 2.5s without heartbeat = dead instance
constexpr uint64_t DIRECTIVE_STALE_TIMEOUT_MS = 1000; // 1s without directive packet = stale

#pragma pack(push, 8)

// ==============================================================================
// Global Taste Broadcast State (Coordinator -> All instances)
// Protected by SharedRegistryHeader::tasteSequence atomic seqlock in SharedRegistryHeader
// ==============================================================================
struct GlobalTasteState {
    uint32_t profileId = 1;         // 1=ModernWorship, 2=AcousticWorship, 3=Gospel, 4=RockPop, >=100=Custom
    char profileName[32] = "Modern Worship";
    float followPercent = 100.0f;   // 0.0 to 100.0 %

    // 5 Macros [-100.0 .. +100.0]
    float macroVocalForward = 0.0f;
    float macroEnergy = 0.0f;
    float macroWarmBright = 0.0f;
    float macroSpace = 0.0f;
    float macroSaturation = 0.0f;

    // Session Mode
    SessionMode sessionMode = SessionMode::Live;

    // Shared FX configuration
    ReverbType reverbType = ReverbType::Hall;
    float reverbDecaySec = 3.2f;
    float reverbPredelayMs = 20.0f;
    float reverbDamping = 0.5f;
    DelaySubdivision delaySubdivision = DelaySubdivision::DottedEighth;
    float delayFeedback = 0.35f;
    bool delayPingPong = true;

    // Reverb & Delay sends per FxCategory (8 categories)
    float reverbSendDb[NUM_FX_CATEGORIES] = {0};
    float delaySendDb[NUM_FX_CATEGORIES] = {0};

    // Master Tonal EQ deltas
    float masterLowShelfDb = 0.0f;
    float masterPresenceDb = 0.0f;
    float masterAirShelfDb = 0.0f;

    // Global Scalars
    float maskingScale = 1.0f;
    float warmthScale = 1.0f;
    float transientScale = 1.0f;
    float widthScale = 1.0f;

    // Playbook / solver deltas (21 instrument slots)
    float rideOffsetDelta[NUM_INSTRUMENT_SLOTS] = {0};
    float characterTiltDb[NUM_INSTRUMENT_SLOTS] = {0};
};

// ==============================================================================
// Live Telemetry Payload (published at 10-20 Hz by each instance)
// ==============================================================================
struct InstanceTelemetry {
    uint32_t instanceId = 0;
    uint32_t processId = 0;
    uint64_t registrationTimestampMs = 0;
    uint64_t lastHeartbeatMs = 0;
    
    PluginRole role = PluginRole::Source;
    bool isCoordinator = false;
    DestinationTag destination = DestinationTag::PALive;
    BusType busType = BusType::Generic;
    uint8_t priority = 50; // 0 to 100
    
    char trackName[64] = {0};
    SideHint sideHint = SideHint::Centre;
    InstrumentType instrumentType = InstrumentType::Unknown;
    float instrumentConfidence = 0.0f; // 0.0 to 1.0
    IdentitySource identitySource = IdentitySource::None;
    
    bool signalPresent = false;
    float rmsDb = -100.0f;
    float peakDb = -100.0f;
    float momentaryLufs = -100.0f;
    float shortTermLufs = -100.0f;
    float integratedLufs = -100.0f;
    float crestFactorDb = 0.0f;
    float fundamentalHz = 0.0f;
    float transientRate = 0.0f;
    float isPercussive = 0.0f;          // 0.0 (sustained harmonic) to 1.0 (transient percussive)
    float spectralCentroidHz = 0.0f;    // In Hz
    float stereoWidth = 1.0f;
    float stereoCorrelation = 1.0f;
    
    PanPosition currentPan;
    float spectrumBands[NUM_SPECTRUM_BANDS] = {0}; // Normalized 0.0 to 1.0
    
    // Local DSP current states
    float currentTrimDb = 0.0f;
    bool autoTrimAtLimit = false;       // Set when auto-trim is pinned at the +/-12 dB cap
    float currentCompGrDb = 0.0f;
    float currentRideOffsetDb = 0.0f;
    float limiterGrDb = 0.0f;
    float tonalCorrectionDb = 0.0f;
    bool narrowbandWarning = false;
    bool solverCutsActive = false;
    bool solverDuckActive = false;
    uint32_t manualOverrides = ManualOverride::None;
    bool isFrozen = false;
    bool isBypassed = false;

    // MVP 5 Taste & Local FX
    float moreLessDb = 0.0f;            // "More/Less of Me" [-3.0 dB, +3.0 dB]
    bool localTailEnabled = true;       // Source role local FX tail mode
    float localSendTrimDb = 0.0f;       // User trim on FX sends [-12.0 dB, +6.0 dB]

    // Profile learning proposals (Source -> Coordinator)
    uint32_t proposalSequence = 0;
    float proposalRideDelta = 0.0f;
    float proposalWarmthScale = 1.0f;
    float proposalSendTrimDb = 0.0f;
    float proposalCharacterTiltDb = 0.0f;
};

// ==============================================================================
// Directives Sent from Coordinator to Each Instance (Solver output)
// ==============================================================================
struct InstanceDirectives {
    uint32_t solverSequence = 0;
    uint64_t directiveTimestampMs = 0;
    bool solverEnabled = true;

    float rideOffsetDb = 0.0f;     // Balance ride [-4.0 dB, +4.0 dB]
    float solverTrimDb = 0.0f;     // Global staging correction [-3.0 dB, +2.0 dB]
    float duckDb = 0.0f;           // Telemetry envelope ducking (Kick -> Bass) [-3.0 dB, 0.0 dB]
    uint32_t duckKeyInstanceId = 0;

    PanPosition panTarget;         // Reserved for future
    MaskingCutDirective maskingCuts[4];

    // MVP 5 Directives
    float reverbSendDb = -60.0f;   // Target send level
    float delaySendDb = -60.0f;
    float characterTiltDb = 0.0f;
    float warmthTarget = 0.0f;
    SessionMode sessionMode = SessionMode::Live;
};

// ==============================================================================
// Shared Slot layout in Shared Memory
// Protected by atomic sequence lock (seqlock)
// ==============================================================================
struct InstanceSlot {
    std::atomic<uint32_t> sequence{0};   // Even = consistent, Odd = write in progress
    std::atomic<uint32_t> slotActive{0}; // 0 = empty/free, 1 = occupied
    
    InstanceTelemetry telemetry;
    InstanceDirectives directives;
};

// ==============================================================================
// Entire Shared Registry Memory Block Layout
// ==============================================================================
struct SharedRegistryHeader {
    uint32_t magic = SHARED_MAGIC;
    uint32_t version = PROTOCOL_VERSION;
    uint32_t maxSlots = static_cast<uint32_t>(MAX_REGISTRY_INSTANCES);
    uint32_t slotSize = sizeof(InstanceSlot);
    
    std::atomic<uint32_t> coordinatorInstanceId{0};
    std::atomic<uint64_t> lastSweepTimestampMs{0};
    std::atomic<uint32_t> activeInstanceCount{0};

    // MVP 5 Global Taste state broadcast
    std::atomic<uint32_t> tasteSequence{0};
    GlobalTasteState tasteState;
};

struct SharedRegistryBlock {
    SharedRegistryHeader header;
    InstanceSlot slots[MAX_REGISTRY_INSTANCES];
};

#pragma pack(pop)

// 32-Band Center Frequencies for Worship & Pro Mixing (approx 1/3 octave spacing from 25Hz to 20kHz)
constexpr float SPECTRUM_BAND_FREQUENCIES[NUM_SPECTRUM_BANDS] = {
    25.0f,   31.5f,  40.0f,   50.0f,   63.0f,   80.0f,   100.0f,  125.0f,
    160.0f,  200.0f, 250.0f,  315.0f,  400.0f,  500.0f,  630.0f,  800.0f,
    1000.0f, 1250.0f, 1600.0f, 2000.0f, 2500.0f, 3150.0f, 4000.0f, 5000.0f,
    6300.0f, 8000.0f, 10000.0f, 12500.0f, 14000.0f, 16000.0f, 18000.0f, 20000.0f
};

} // namespace aimixer
