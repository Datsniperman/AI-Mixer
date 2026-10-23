#include "MixSolver.h"
#include "TasteProfile.h"
#include <algorithm>
#include <cmath>

namespace aimixer {

float MixSolver::calculateVocalForwardDelta(InstrumentType type, float macroVocalForward) noexcept {
    if (std::abs(macroVocalForward) < 0.001f) return 0.0f;
    float norm = macroVocalForward / 100.0f; // -1.0 to +1.0
    if (type == InstrumentType::LeadVocal) {
        return norm * 1.5f; // +100 -> +1.5 dB, -100 -> -1.5 dB
    } else {
        if (norm > 0.0f) {
            return -norm * 1.5f; // +100 -> -1.5 dB
        } else {
            return -norm * 1.0f; // -100 -> +1.0 dB
        }
    }
}

float MixSolver::calculateEnergyMaskingScalar(float macroEnergy, float profileMaskingScale, float followPercent) noexcept {
    float f = std::clamp(followPercent / 100.0f, 0.0f, 1.0f);
    float effectiveMaskingScale = 1.0f + (profileMaskingScale - 1.0f) * f;
    float energyFactor = 1.0f + (macroEnergy / 100.0f) * 0.3f; // -100 -> 0.7x, +100 -> 1.3x
    return std::max(0.1f, effectiveMaskingScale * energyFactor);
}

float MixSolver::calculateWarmthScalar(float macroSaturation, float profileWarmthScale, float followPercent) noexcept {
    float f = std::clamp(followPercent / 100.0f, 0.0f, 1.0f);
    float effectiveWarmthScale = 1.0f + (profileWarmthScale - 1.0f) * f;
    float satFactor = 1.0f + (macroSaturation / 100.0f) * 0.8f; // -100 -> 0.2x, +100 -> 1.8x
    return std::max(0.0f, effectiveWarmthScale * satFactor);
}

uint8_t MixSolver::getInstrumentPriority(InstrumentType type) noexcept {
    switch (type) {
        case InstrumentType::LeadVocal:       return 100;
        case InstrumentType::PastorSpeaking:  return 95;
        case InstrumentType::Kick:            return 85;
        case InstrumentType::Bass:            return 85;
        case InstrumentType::Snare:           return 80;
        case InstrumentType::Toms:            return 70;
        case InstrumentType::Piano:           return 60;
        case InstrumentType::Keys:            return 60;
        case InstrumentType::BackgroundVocal: return 60;
        case InstrumentType::Choir:           return 60;
        case InstrumentType::AcousticGuitar:  return 55;
        case InstrumentType::ElectricGuitar:  return 55;
        case InstrumentType::Overheads:       return 50;
        case InstrumentType::HiHat:           return 50;
        case InstrumentType::Unknown:         return 50;
        case InstrumentType::Synth:           return 45;
        case InstrumentType::Tracks:          return 40;
        case InstrumentType::Pad:             return 30;
        case InstrumentType::Room:            return 20;
        case InstrumentType::Click:           return 0; // Click is excluded
        default:                              return 50;
    }
}

float MixSolver::getRelativeRideTarget(InstrumentType type, bool isSpeechMode) noexcept {
    float speechPenalty = isSpeechMode ? -4.0f : 0.0f;

    switch (type) {
        case InstrumentType::LeadVocal:       return 0.0f;
        case InstrumentType::PastorSpeaking:  return 0.0f;
        case InstrumentType::Kick:            return -3.0f;
        case InstrumentType::Snare:           return -4.0f;
        case InstrumentType::Bass:            return -5.0f;
        case InstrumentType::BackgroundVocal:
        case InstrumentType::Choir:           return -6.0f;
        case InstrumentType::Piano:
        case InstrumentType::Keys:            return -7.0f + speechPenalty;
        case InstrumentType::Toms:            return -7.0f;
        case InstrumentType::Tracks:          return -8.0f + speechPenalty;
        case InstrumentType::AcousticGuitar:
        case InstrumentType::ElectricGuitar:  return -9.0f + speechPenalty;
        case InstrumentType::Synth:           return -10.0f + speechPenalty;
        case InstrumentType::Overheads:       return -10.0f;
        case InstrumentType::Unknown:         return -10.0f;
        case InstrumentType::HiHat:           return -12.0f;
        case InstrumentType::Pad:             return -14.0f + speechPenalty;
        case InstrumentType::Room:            return -20.0f;
        case InstrumentType::Click:
        default:                              return 0.0f;
    }
}

float MixSolver::getCutQForFrequency(float freqHz) noexcept {
    if (freqHz < 400.0f) return 1.0f;
    if (freqHz > 2000.0f) return 2.2f;
    return 1.4f;
}

bool MixSolver::isBandInPairRegion(size_t bandIdx, InstrumentType winner, InstrumentType loser) {
    if (bandIdx >= NUM_SPECTRUM_BANDS) return false;
    float freq = SPECTRUM_BAND_FREQUENCIES[bandIdx];

    // Explicit Pair Table band filters
    if (winner == InstrumentType::LeadVocal) {
        if (loser == InstrumentType::BackgroundVocal || loser == InstrumentType::Choir) {
            return (freq >= 2000.0f && freq <= 5000.0f);
        }
        if (loser == InstrumentType::Snare) {
            return (freq >= 2000.0f && freq <= 5000.0f);
        }
        return (freq >= 300.0f && freq <= 5000.0f);
    }

    if (winner == InstrumentType::PastorSpeaking) {
        return (freq >= 300.0f && freq <= 4000.0f);
    }

    if (winner == InstrumentType::Piano && loser == InstrumentType::AcousticGuitar) {
        return (freq >= 800.0f && freq <= 3000.0f);
    }

    if (loser == InstrumentType::Pad) {
        return (freq >= 200.0f && freq <= 4000.0f);
    }

    // Default region for other high-priority collisions
    return (freq >= 250.0f && freq <= 5000.0f);
}

std::unordered_map<uint32_t, InstanceDirectives> MixSolver::solve(
    const std::vector<InstanceTelemetry>& snapshot,
    uint64_t nowMs,
    SolverState& state,
    const GlobalTasteState& tasteState) {
    
    std::unordered_map<uint32_t, InstanceDirectives> result;
    state.solverSequence++;

    // Step 1: Filter participating active sources
    std::vector<const InstanceTelemetry*> activeSources;
    activeSources.reserve(snapshot.size());

    int currentActiveCount = 0;
    float totalLinearSum = 0.0f;

    for (const auto& inst : snapshot) {
        // Exclude Click, Master, Bus
        if (inst.role != PluginRole::Source) continue;
        if (inst.instrumentType == InstrumentType::Click) continue;

        // Must have signal present
        if (inst.signalPresent && inst.rmsDb > -55.0f) {
            activeSources.push_back(&inst);
            currentActiveCount++;
            totalLinearSum += std::pow(10.0f, inst.rmsDb / 20.0f);
        }

        // Initialize default packet
        InstanceDirectives dir;
        dir.solverSequence = state.solverSequence;
        dir.directiveTimestampMs = nowMs;
        dir.solverEnabled = state.globalSolverEnabled;
        dir.sessionMode = tasteState.sessionMode;
        result[inst.instanceId] = dir;
    }

    if (!state.globalSolverEnabled || activeSources.empty()) {
        return result;
    }

    // Arrangement Chorus Detection
    float totalRmsDb = (totalLinearSum > 1e-5f) ? (20.0f * std::log10(totalLinearSum)) : -100.0f;
    if (state.prevActiveCount > 0 &&
        currentActiveCount >= state.prevActiveCount + 3 &&
        totalRmsDb >= state.prevTotalLevel + 6.0f) {
        state.chorusFreezeUntilMs = nowMs + 4000; // Freeze rides for 4 seconds on chorus arrival
    }
    state.prevActiveCount = currentActiveCount;
    state.prevTotalLevel = totalRmsDb;

    bool isChorusFrozen = (nowMs < state.chorusFreezeUntilMs);
    bool isFreezeAll = (tasteState.sessionMode == SessionMode::FreezeAll);
    float follow = std::clamp(tasteState.followPercent / 100.0f, 0.0f, 1.0f);

    // Step 2: Reference Selection (Melodic / Vocal Anchor)
    const InstanceTelemetry* referenceInstance = nullptr;
    bool isSpeechMode = false;
    ReasonCode refReason = ReasonCode::RideTarget;

    // 2.1 Check Lead Vocal
    for (const auto* s : activeSources) {
        if (s->instrumentType == InstrumentType::LeadVocal) {
            referenceInstance = s;
            break;
        }
    }

    // 2.2 Check Pastor (Speech Mode)
    if (referenceInstance == nullptr) {
        for (const auto* s : activeSources) {
            if (s->instrumentType == InstrumentType::PastorSpeaking) {
                referenceInstance = s;
                isSpeechMode = true;
                break;
            }
        }
    }

    // 2.3 Solo Promotion: Promote highest priority active source to 0 dB reference
    if (referenceInstance == nullptr) {
        uint8_t highestPrio = 0;
        for (const auto* s : activeSources) {
            uint8_t prio = getInstrumentPriority(s->instrumentType);
            if (prio > highestPrio) {
                highestPrio = prio;
                referenceInstance = s;
                refReason = ReasonCode::RideSoloPromote;
            }
        }
    }

    float referenceSlowRms = (referenceInstance != nullptr) ? referenceInstance->rmsDb : -18.0f;

    // Step 3: Balance Rides & Global Headroom Staging
    for (const auto* inst : activeSources) {
        auto& instState = state.instanceStates[inst->instanceId];

        // 3-5 sec slow smoothed RMS
        if (instState.slowSmoothedRms < -90.0f) {
            instState.slowSmoothedRms = inst->rmsDb;
        } else {
            instState.slowSmoothedRms = 0.90f * instState.slowSmoothedRms + 0.10f * inst->rmsDb;
        }

        // Global Staging: Headroom guard (peak > -3 dBFS sustained)
        if (!isFreezeAll) {
            if (inst->peakDb > -3.0f) {
                instState.hotPeakCycles++;
                if (instState.hotPeakCycles >= 3) {
                    instState.currentSolverTrimDb = std::max(-3.0f, instState.currentSolverTrimDb - 1.0f);
                    instState.hotPeakCycles = 0;
                }
            } else {
                instState.hotPeakCycles = 0;
            }
        }

        // Staging: Persistently buried source (> 10 dB below ride target for > 5s)
        float baseTargetOffset = getRelativeRideTarget(inst->instrumentType, isSpeechMode);
        size_t instSlot = static_cast<size_t>(inst->instrumentType);
        float profileDelta = (instSlot < NUM_INSTRUMENT_SLOTS) ? (tasteState.rideOffsetDelta[instSlot] * follow) : 0.0f;
        float vocalForwardDelta = calculateVocalForwardDelta(inst->instrumentType, tasteState.macroVocalForward);
        float targetOffset = baseTargetOffset + profileDelta + vocalForwardDelta + inst->moreLessDb;

        float measuredOffset = instState.slowSmoothedRms - referenceSlowRms;
        if (!isFreezeAll) {
            if (measuredOffset < targetOffset - 10.0f) {
                instState.lowRmsSeconds += 0.2f;
                if (instState.lowRmsSeconds >= 5.0f) {
                    instState.currentSolverTrimDb = std::min(2.0f, instState.currentSolverTrimDb + 0.5f);
                    instState.lowRmsSeconds = 0.0f;
                }
            } else {
                instState.lowRmsSeconds = 0.0f;
            }
        }

        // Balance Ride Target
        if (isFreezeAll) {
            // In FreezeAll mode, hold current ride offset strictly static
        } else if (inst == referenceInstance) {
            instState.currentRideDb = 0.0f; // Reference track has 0 ride offset
        } else if (!isChorusFrozen) {
            float rawDelta = targetOffset - measuredOffset;

            // Deadband +/- 1.0 dB: inside deadband, hold steady
            if (std::abs(rawDelta) > 1.0f) {
                float targetRide = std::clamp(rawDelta, -4.0f, 4.0f);
                // Rate-limit: Live mode max 0.5 dB/sec (0.10 dB/step), Learn mode max 2.0 dB/sec (0.20 dB/step)
                float maxStep = (tasteState.sessionMode == SessionMode::Learn) ? 0.20f : 0.10f;
                float step = std::clamp(targetRide - instState.currentRideDb, -maxStep, maxStep);
                instState.currentRideDb += step;
            }
        }

        auto& dir = result[inst->instanceId];
        dir.rideOffsetDb = std::clamp(instState.currentRideDb, -4.0f, 4.0f);
        dir.solverTrimDb = std::clamp(instState.currentSolverTrimDb, -3.0f, 2.0f);
        dir.sessionMode = tasteState.sessionMode;

        // Shared FX sends
        FxCategory cat = TasteProfile::getCategoryForInstrument(inst->instrumentType);
        size_t catIdx = static_cast<size_t>(cat);
        float baseRev = (catIdx < NUM_FX_CATEGORIES) ? tasteState.reverbSendDb[catIdx] : -60.0f;
        float baseDly = (catIdx < NUM_FX_CATEGORIES) ? tasteState.delaySendDb[catIdx] : -60.0f;
        if (!TasteProfile::isFxEnabledByDefault(inst->instrumentType)) {
            baseRev = -100.0f;
            baseDly = -100.0f;
        }

        float spaceDelta = 0.0f;
        if (tasteState.macroSpace > 0.0f) {
            spaceDelta = (tasteState.macroSpace / 100.0f) * 3.0f;
        } else if (tasteState.macroSpace < 0.0f) {
            spaceDelta = (tasteState.macroSpace / 100.0f) * 4.0f;
        }

        if (baseRev > -50.0f || inst->localSendTrimDb > 0.0f) {
            dir.reverbSendDb = std::clamp(baseRev + spaceDelta + inst->localSendTrimDb, -60.0f, 0.0f);
        } else {
            dir.reverbSendDb = -60.0f;
        }

        if (baseDly > -50.0f || inst->localSendTrimDb > 0.0f) {
            dir.delaySendDb = std::clamp(baseDly + spaceDelta + inst->localSendTrimDb, -60.0f, 0.0f);
        } else {
            dir.delaySendDb = -60.0f;
        }

        // Character tilt
        float baseTilt = (instSlot < NUM_INSTRUMENT_SLOTS) ? (tasteState.characterTiltDb[instSlot] * follow) : 0.0f;
        float macroTilt = (tasteState.macroWarmBright / 100.0f) * 1.5f;
        dir.characterTiltDb = std::clamp(baseTilt + macroTilt, -4.0f, 4.0f);

        // Warmth target scalar
        dir.warmthTarget = calculateWarmthScalar(tasteState.macroSaturation, tasteState.warmthScale, tasteState.followPercent);
    }

    // Step 4: Masking Collisions & Carving Cuts
    // Temporary container to accumulate cuts per target instance
    std::unordered_map<uint32_t, std::vector<MaskingCutDirective>> instanceCuts;

    for (size_t i = 0; i < activeSources.size(); ++i) {
        for (size_t j = 0; j < activeSources.size(); ++j) {
            if (i == j) continue;

            const auto* s1 = activeSources[i];
            const auto* s2 = activeSources[j];

            // Determine Winner and Loser by priority (biased by More/Less of Me)
            int prioBonus1 = std::clamp(static_cast<int>(std::round(s1->moreLessDb * 3.0f)), -10, 10);
            int prioBonus2 = std::clamp(static_cast<int>(std::round(s2->moreLessDb * 3.0f)), -10, 10);
            uint8_t prio1 = static_cast<uint8_t>(std::clamp(static_cast<int>(getInstrumentPriority(s1->instrumentType)) + prioBonus1, 0, 100));
            uint8_t prio2 = static_cast<uint8_t>(std::clamp(static_cast<int>(getInstrumentPriority(s2->instrumentType)) + prioBonus2, 0, 100));

            // Click, Bus, Master never masked
            if (s2->instrumentType == InstrumentType::Click || s2->role != PluginRole::Source) continue;

            float maskScalar = calculateEnergyMaskingScalar(tasteState.macroEnergy, tasteState.maskingScale, tasteState.followPercent);
            float minCutCap = (tasteState.sessionMode == SessionMode::Learn) ? -8.0f : -6.0f;

            // Special Pair: Kick <-> Bass
            if ((s1->instrumentType == InstrumentType::Kick && s2->instrumentType == InstrumentType::Bass)) {
                // Bass cut at Kick fundamental (40-90 Hz, Q 1.0)
                float kickFund = (s1->fundamentalHz >= 40.0f && s1->fundamentalHz <= 100.0f) ? s1->fundamentalHz : 60.0f;
                MaskingCutDirective cut;
                cut.enabled = true;
                cut.freqHz = kickFund;
                cut.gainDb = std::clamp(-3.5f * maskScalar, minCutCap, -0.5f); // Clamped negative cut
                cut.q = 1.0f;
                cut.reason = ReasonCode::MaskingKick;
                cut.otherInstanceId = s1->instanceId;
                cut.otherInstrument = s1->instrumentType;
                instanceCuts[s2->instanceId].push_back(cut);

                // Telemetry Envelope Ducking: Bass ducks 1.0 to 3.0 dB proportional to Kick level
                float duckAmount = std::clamp((s1->rmsDb - (-35.0f)) / 15.0f * 2.0f + 1.0f, 1.0f, 3.0f) * maskScalar;
                result[s2->instanceId].duckDb = -duckAmount;
                result[s2->instanceId].duckKeyInstanceId = s1->instanceId;
                continue;
            }

            if ((s1->instrumentType == InstrumentType::Bass && s2->instrumentType == InstrumentType::Kick)) {
                // Kick cuts at Bass fundamental IF Bass fundamental > 90 Hz
                if (s1->fundamentalHz >= 90.0f && s1->fundamentalHz <= 180.0f) {
                    MaskingCutDirective cut;
                    cut.enabled = true;
                    cut.freqHz = s1->fundamentalHz;
                    cut.gainDb = std::clamp(-2.5f * maskScalar, minCutCap, -0.5f);
                    cut.q = 1.4f;
                    cut.reason = ReasonCode::MaskingBass;
                    cut.otherInstanceId = s1->instanceId;
                    cut.otherInstrument = s1->instrumentType;
                    instanceCuts[s2->instanceId].push_back(cut);
                }
                continue;
            }

            // Normal Priority Rules: s1 must have higher priority than s2 to win
            if (prio1 <= prio2) continue;

            const auto* winner = s1;
            const auto* loser = s2;

            // Unknown instances can never win a masking conflict
            if (winner->instrumentType == InstrumentType::Unknown) continue;

            uint32_t pairKey = (winner->instanceId << 16) | (loser->instanceId & 0xFFFF);
            auto& pairState = state.pairStates[pairKey];

            // Evaluate 32-band spectrum collision
            float maxOverlap = 0.0f;
            size_t bestBand = 0;

            for (size_t b = 0; b < NUM_SPECTRUM_BANDS; ++b) {
                if (!isBandInPairRegion(b, winner->instrumentType, loser->instrumentType)) continue;

                float winVal = winner->spectrumBands[b];
                float loseVal = loser->spectrumBands[b];

                // Both must be meaningfully above floor (> 0.25 normalized) and within ~12 dB (0.35 norm)
                if (winVal > 0.25f && loseVal > 0.25f && std::abs(winVal - loseVal) <= 0.35f) {
                    float overlap = std::min(winVal, loseVal);
                    if (overlap > maxOverlap) {
                        maxOverlap = overlap;
                        bestBand = b;
                    }
                }
            }

            if (maxOverlap > 0.30f) {
                pairState.consecutiveCollisions++;
                pairState.consecutiveClears = 0;
            } else {
                pairState.consecutiveClears++;
                pairState.consecutiveCollisions = 0;
            }

            // Hysteresis: Apply cut only after 3 consecutive cycles (~600 ms)
            if (pairState.consecutiveCollisions >= 3 || nowMs < pairState.holdUntilMs) {
                float rawCut = -std::clamp((maxOverlap - 0.20f) * 8.0f + 1.0f, 1.0f, 6.0f);
                float cutGain = std::clamp(rawCut * maskScalar, minCutCap, -0.5f);

                // Specific caps
                if (loser->instrumentType == InstrumentType::Snare) {
                    cutGain = std::max(-3.0f * maskScalar, cutGain); // Snare cut capped to -3 dB * maskScalar
                } else if (loser->instrumentType == InstrumentType::Pad) {
                    cutGain = std::max(minCutCap, cutGain); // Pad yields up to minCutCap
                }

                // Minimum 1 second hold
                if (pairState.holdUntilMs <= nowMs) {
                    pairState.holdUntilMs = nowMs + 1000;
                }

                ReasonCode rCode = ReasonCode::MaskingGeneric;
                if (winner->instrumentType == InstrumentType::LeadVocal || winner->instrumentType == InstrumentType::PastorSpeaking) {
                    rCode = ReasonCode::MaskingVocal;
                } else if (winner->instrumentType == InstrumentType::Piano || winner->instrumentType == InstrumentType::Keys) {
                    rCode = ReasonCode::MaskingPiano;
                }

                MaskingCutDirective cut;
                cut.enabled = true;
                cut.freqHz = SPECTRUM_BAND_FREQUENCIES[bestBand];
                cut.gainDb = cutGain;
                cut.q = getCutQForFrequency(cut.freqHz);
                cut.reason = rCode;
                cut.otherInstanceId = winner->instanceId;
                cut.otherInstrument = winner->instrumentType;

                instanceCuts[loser->instanceId].push_back(cut);
            }
        }
    }

    // Step 5: Merge and Clamp Cuts (Max 4 per target instance, deepest first)
    for (auto& [instId, cuts] : instanceCuts) {
        if (cuts.empty()) continue;

        // Sort by deepest cut first
        std::sort(cuts.begin(), cuts.end(), [](const MaskingCutDirective& a, const MaskingCutDirective& b) {
            return a.gainDb < b.gainDb; // -6.0 < -3.0 (stronger cut first)
        });

        // Merge nearby cuts within 25% frequency to prevent duplicate notches
        std::vector<MaskingCutDirective> merged;
        for (const auto& c : cuts) {
            bool foundNearby = false;
            for (auto& m : merged) {
                if (std::abs(m.freqHz - c.freqHz) / m.freqHz < 0.25f) {
                    // Keep deepest
                    if (c.gainDb < m.gainDb) {
                        m.gainDb = c.gainDb;
                        m.reason = c.reason;
                    }
                    foundNearby = true;
                    break;
                }
            }
            if (!foundNearby && merged.size() < 4) {
                merged.push_back(c);
            }
        }

        auto& dir = result[instId];
        for (size_t k = 0; k < merged.size() && k < 4; ++k) {
            dir.maskingCuts[k] = merged[k];
        }
    }

    // Step 6: Respect Manual Overrides & Freeze
    for (const auto& inst : snapshot) {
        auto it = result.find(inst.instanceId);
        if (it == result.end()) continue;

        // If frozen or user override active, zero corresponding directives (unless session FreezeAll is active, which holds values)
        if (tasteState.sessionMode != SessionMode::FreezeAll) {
            if (inst.isFrozen || (inst.manualOverrides & ManualOverride::Freeze)) {
                it->second.rideOffsetDb = 0.0f;
                it->second.solverTrimDb = 0.0f;
                it->second.duckDb = 0.0f;
                for (int k = 0; k < 4; ++k) it->second.maskingCuts[k].enabled = false;
            } else {
                if (inst.manualOverrides & ManualOverride::EQ) {
                    for (int k = 0; k < 4; ++k) it->second.maskingCuts[k].enabled = false;
                }
                if (inst.manualOverrides & ManualOverride::LevelRide) {
                    it->second.rideOffsetDb = 0.0f;
                    it->second.duckDb = 0.0f;
                }
                if (inst.manualOverrides & ManualOverride::Trim) {
                    it->second.solverTrimDb = 0.0f;
                }
            }
        }
    }

    return result;
}

} // namespace aimixer
