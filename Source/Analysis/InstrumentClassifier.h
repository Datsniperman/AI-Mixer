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
