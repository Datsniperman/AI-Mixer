#pragma once

#include "../Common/Types.h"
#include "../Common/SharedProtocol.h"
#include "TrackNameParser.h"
#include <vector>

namespace aimixer {

struct AcousticFeatures {
    float rmsDb = -100.0f;
    float peakDb = -100.0f;
    float crestFactorDb = 0.0f;
    float fundamentalHz = 0.0f;
    float spectralCentroidHz = 0.0f;
    float lowFreqRatio = 0.0f;      // < 120 Hz energy share
    float bodyBandRatio = 0.0f;     // 180 - 280 Hz energy share (snare body)
    float midFreqRatio = 0.0f;      // 300 - 3500 Hz energy share (vocal band)
    float highFreqRatio = 0.0f;     // > 5000 Hz energy share
    float isPercussive = 0.0f;      // 0.0 (smooth) to 1.0 (highly percussive)
    float stereoWidth = 1.0f;

    // Advanced Acoustic Hearing (Measured symptoms & characteristics)
    float detectedResonanceHz = 0.0f;      // Prominent narrow ringing / room mode
    float resonanceProminenceDb = 0.0f;    // Prominence above local spectral envelope
    float resonanceQ = 2.0f;               // Estimated sharpness of the resonance
    float sibilanceRatio = 0.0f;           // High-frequency harshness (5.5 - 9 kHz vs 800 - 2.5 kHz)
    float sibilanceCentroidHz = 7000.0f;   // Measured center of harsh sibilant bursts
    float spectralTiltDbPerOct = -4.5f;    // Slope of the spectral curve (dark < -6.0, bright > -3.0)
    float boxinessRatio = 0.0f;            // Mud/boxiness concentration in 350 - 700 Hz
    float subEnergyRatio = 0.0f;           // Sub-bass energy concentration in 20 - 60 Hz
    float harmonicRichness = 0.0f;         // 0.0 = pure/sterile, 1.0 = harmonically dense/saturated
    float attackSharpness = 0.0f;          // Measured transient spike energy in first 10ms
};

struct ClassificationResult {
    InstrumentType type = InstrumentType::Unknown;
    float confidence = 0.0f;
    bool isLocked = false;
};

class InstrumentClassifier {
public:
    InstrumentClassifier();

    // Runs on worker/analysis thread at ~2 Hz
    ClassificationResult classify(const AcousticFeatures& features);

    // Context / Layer 4 Helper: demotes duplicate LeadVocal to BGV or assists drums
    static ClassificationResult applyNeighbourContext(
        const ClassificationResult& localResult,
        const std::vector<InstanceTelemetry>& sessionSnapshot,
        uint32_t currentInstanceId
    );

    // Master Precedence Resolver: User > TrackName (>=0.80) > Classifier (>=0.60) > Unknown
    static InstrumentType resolveIdentity(
        InstrumentType userOverride,
        const ParseResult& trackNameResult,
        const ClassificationResult& classifiedResult,
        IdentitySource& outSource,
        float& outConfidence
    );

    void reset();

private:
    int consecutiveConfidenceHits = 0;
    InstrumentType lastCandidateType = InstrumentType::Unknown;
    ClassificationResult lockedClassification;
    float silenceSeconds = 0.0f;
};

} // namespace aimixer
