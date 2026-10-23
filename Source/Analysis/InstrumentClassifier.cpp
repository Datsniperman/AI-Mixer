#include "InstrumentClassifier.h"
#include <algorithm>
#include <cmath>

namespace aimixer {

InstrumentClassifier::InstrumentClassifier() {
    reset();
}

void InstrumentClassifier::reset() {
    consecutiveConfidenceHits = 0;
    lastCandidateType = InstrumentType::Unknown;
    lockedClassification = { InstrumentType::Unknown, 0.0f, false };
    silenceSeconds = 0.0f;
}

ClassificationResult InstrumentClassifier::classify(const AcousticFeatures& f) {
    // Rule: Silence / Near-silent (<-55 dBFS) - do not flip or classify
    if (f.rmsDb < -55.0f) {
        silenceSeconds += 0.5f; // ~2 Hz update rate = 0.5s per cycle
        if (silenceSeconds > 10.0f) {
            // Unlock after 10 seconds of continuous silence
            lockedClassification = { InstrumentType::Unknown, 0.0f, false };
            consecutiveConfidenceHits = 0;
        }
        return lockedClassification;
    }

    silenceSeconds = 0.0f;

    // Return locked classification if firmly locked
    if (lockedClassification.isLocked) {
        return lockedClassification;
    }

    // Scored Rule Table (Transparent Decision Tree)
    InstrumentType bestType = InstrumentType::Unknown;
    float bestConfidence = 0.0f;

    auto updateCandidate = [&](InstrumentType type, float conf) {
        if (conf > bestConfidence) {
            bestConfidence = conf;
            bestType = type;
        }
    };

    // RULE 1: Kick
    // Strong energy below 120 Hz, highly percussive, low fundamental/sub thump
    if (f.lowFreqRatio > 0.40f && f.isPercussive > 0.55f && (f.fundamentalHz < 110.0f || f.fundamentalHz == 0.0f)) {
        float kickConf = 0.85f + 0.15f * std::clamp((f.lowFreqRatio - 0.40f) / 0.40f, 0.0f, 1.0f);
        updateCandidate(InstrumentType::Kick, kickConf);
    }

    // RULE 2: Snare
    // Percussive onset with body (180-280 Hz) and crack/wire centroid (1100 - 4500 Hz), low sub
    if (f.isPercussive > 0.50f && f.spectralCentroidHz >= 1100.0f && f.spectralCentroidHz <= 4500.0f && (f.lowFreqRatio < 0.25f || f.bodyBandRatio > 0.12f)) {
        updateCandidate(InstrumentType::Snare, 0.85f);
    }

    // RULE 3: Hi-Hat
    // Highly percussive, high spectral centroid (> 5500 Hz), almost zero sub bass
    if (f.isPercussive > 0.45f && f.spectralCentroidHz > 5500.0f && f.lowFreqRatio < 0.08f) {
        // Guard (a): Overheads-like signal (high stereo width + high air) blocks Hi-Hat lock
        bool isOverheadsLike = (f.stereoWidth > 0.80f && f.highFreqRatio > 0.30f);
        float hatConf = isOverheadsLike ? 0.70f : 0.88f; // Capped at 0.70 (non-locking)
        updateCandidate(InstrumentType::HiHat, hatConf);
    }

    // RULE 3b: Conservative Piano Candidate (Capped at 0.70 confidence, never lock-eligible)
    // Sustained harmonic, moderate onset, fundamental 50-1000 Hz, centroid 800-2500 Hz
    if (f.isPercussive < 0.35f && f.crestFactorDb < 8.0f && f.fundamentalHz >= 50.0f && f.fundamentalHz <= 1000.0f &&
        f.spectralCentroidHz >= 800.0f && f.spectralCentroidHz <= 2500.0f && f.lowFreqRatio < 0.30f) {
        updateCandidate(InstrumentType::Piano, 0.70f);
    }

    // RULE 4: Bass
    // Sustained/non-percussive low end, dominant fundamental in 35-180 Hz, centroid below 900 Hz
    if (f.isPercussive < 0.35f && f.fundamentalHz >= 35.0f && f.fundamentalHz <= 180.0f && f.lowFreqRatio > 0.35f && f.spectralCentroidHz < 900.0f) {
        updateCandidate(InstrumentType::Bass, 0.92f);
    }

    // RULE 5: Vocal Candidate (Lead Vocal / Speech / BGV)
    // Human vocal fundamental 80 - 450 Hz, high energy concentration in speech band (300 - 3500 Hz), centroid 1000 - 3600 Hz
    if (f.isPercussive < 0.40f && f.fundamentalHz >= 75.0f && f.fundamentalHz <= 450.0f && f.midFreqRatio > 0.45f && f.spectralCentroidHz >= 950.0f && f.spectralCentroidHz <= 3600.0f) {
        updateCandidate(InstrumentType::LeadVocal, 0.85f);
    }

    // RULE 6: Acoustic Guitar
    // Strum transients (0.25 - 0.60 percussive), bright body & air (1500 - 4200 Hz centroid), significant high frequency
    if (f.isPercussive >= 0.25f && f.isPercussive <= 0.65f && f.spectralCentroidHz >= 1400.0f && f.spectralCentroidHz <= 4200.0f && f.highFreqRatio > 0.12f && f.lowFreqRatio < 0.15f) {
        updateCandidate(InstrumentType::AcousticGuitar, 0.78f);
    }

    // RULE 7: Electric Guitar
    // Moderate/sustained, fundamental in 80 - 600 Hz, cab-filtered roll-off above 6 kHz (low highFreqRatio)
    if (f.isPercussive < 0.50f && f.fundamentalHz >= 80.0f && f.fundamentalHz <= 600.0f && f.spectralCentroidHz >= 850.0f && f.spectralCentroidHz <= 2800.0f && f.highFreqRatio < 0.12f) {
        updateCandidate(InstrumentType::ElectricGuitar, 0.75f);
    }

    // RULE 8: Pad / Ambient Bed
    // Very low transient (slow attack, smooth decay), wide stereo, smooth midrange centroid
    if (f.isPercussive < 0.15f && f.stereoWidth > 0.75f && f.spectralCentroidHz >= 350.0f && f.spectralCentroidHz <= 3200.0f && f.lowFreqRatio < 0.15f) {
        updateCandidate(InstrumentType::Pad, 0.82f);
    }

    // RULE 9: Click (Pass-Through)
    // Extremely sharp percussive spike, very high crest factor (> 18 dB), sparse hits
    if (f.isPercussive > 0.85f && f.crestFactorDb > 18.0f && f.rmsDb < -20.0f) {
        updateCandidate(InstrumentType::Click, 0.95f);
    }

    // Hard Rule: Threshold < 0.60 = Unknown
    if (bestConfidence < 0.60f) {
        return { InstrumentType::Unknown, 0.0f, false };
    }

    ClassificationResult result { bestType, bestConfidence, false };

    // Lock-in filter: require 3 consecutive high-confidence hits (>= 0.85) of the SAME instrument to lock
    if (result.confidence >= 0.85f) {
        if (result.type == lastCandidateType) {
            consecutiveConfidenceHits++;
        } else {
            consecutiveConfidenceHits = 1;
            lastCandidateType = result.type;
        }

        if (consecutiveConfidenceHits >= 3) {
            lockedClassification = result;
            lockedClassification.isLocked = true;
            return lockedClassification;
        }
    } else {
        consecutiveConfidenceHits = 0;
        lastCandidateType = InstrumentType::Unknown;
    }

    return result;
}

ClassificationResult InstrumentClassifier::applyNeighbourContext(
    const ClassificationResult& localResult,
    const std::vector<InstanceTelemetry>& sessionSnapshot,
    uint32_t currentInstanceId) {
    
    ClassificationResult adjusted = localResult;
    if (adjusted.type == InstrumentType::Unknown) return adjusted;

    // Layer 4 rule: If this instance classified as LeadVocal, but another instance in the session
    // is already a confirmed LeadVocal (from user or track name or earlier lock), demote this one to BGV
    if (adjusted.type == InstrumentType::LeadVocal) {
        for (const auto& other : sessionSnapshot) {
            if (other.instanceId != currentInstanceId && other.instrumentType == InstrumentType::LeadVocal && other.instrumentConfidence >= 0.80f) {
                adjusted.type = InstrumentType::BackgroundVocal;
                adjusted.confidence = 0.80f;
                break;
            }
        }
    }

    return adjusted;
}

InstrumentType InstrumentClassifier::resolveIdentity(
    InstrumentType userOverride,
    const ParseResult& trackNameResult,
    const ClassificationResult& classifiedResult,
    IdentitySource& outSource,
    float& outConfidence) {
    
    // Layer 2: User selection is permanent and always wins
    if (userOverride != InstrumentType::Unknown) {
        outSource = IdentitySource::User;
        outConfidence = 1.0f;
        return userOverride;
    }

    // Layer 1: Track-name parser with confidence >= 0.80
    if (trackNameResult.confidence >= 0.80f && trackNameResult.type != InstrumentType::Unknown) {
        outSource = IdentitySource::TrackName;
        outConfidence = trackNameResult.confidence;
        return trackNameResult.type;
    }

    // Layer 3: Audio classifier with confidence >= 0.60
    if (classifiedResult.confidence >= 0.60f && classifiedResult.type != InstrumentType::Unknown) {
        outSource = IdentitySource::Classified;
        outConfidence = classifiedResult.confidence;
        return classifiedResult.type;
    }

    // Fallback: Safe Generic Chain
    outSource = IdentitySource::None;
    outConfidence = 0.0f;
    return InstrumentType::Unknown;
}

} // namespace aimixer
