#include "ProfileManager.h"
#include "../Common/LoadTrace.h"
#include <fstream>

namespace aimixer {

static std::string sanitizeFilename(const std::string& name) {
    std::string out;
    for (char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_') {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    return out.empty() ? "Profile" : out;
}

ProfileManager::ProfileManager() {
    builtInProfiles = TasteProfile::getBuiltInProfiles();
    ensureProfilesDirectoryExists();
    scanProfilesDirectory();
}

juce::File ProfileManager::getProfilesDirectory() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("AI Mixer")
        .getChildFile("Profiles");
}

void ProfileManager::ensureProfilesDirectoryExists() {
    auto dir = getProfilesDirectory();
    if (!dir.exists()) {
        dir.createDirectory();
    }
}

void ProfileManager::scanProfilesDirectory() {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    customProfiles.clear();

    try {
        auto dir = getProfilesDirectory();
        if (!dir.isDirectory()) return;

        auto files = dir.findChildFiles(juce::File::findFiles, false, "*.json");
        for (const auto& file : files) {
            try {
                auto content = file.loadFileAsString().toStdString();
                auto parsed = TasteProfile::fromJsonString(content);
                if (parsed.has_value()) {
                    // Built-in names cannot be overwritten by disk files
                    bool matchesBuiltIn = false;
                    for (const auto& bp : builtInProfiles) {
                        if (bp.profileId == parsed->profileId) {
                            matchesBuiltIn = true;
                            break;
                        }
                    }
                    if (!matchesBuiltIn) {
                        customProfiles.push_back(parsed.value());
                    }
                } else {
                    traceLog("Warning: Malformed profile JSON skipped: %s", file.getFileName().toRawUTF8());
                }
            } catch (const std::exception& e) {
                traceLog("Warning: Exception parsing profile '%s': %s", file.getFileName().toRawUTF8(), e.what());
            } catch (...) {
                traceLog("Warning: Unknown exception parsing profile '%s'", file.getFileName().toRawUTF8());
            }
        }
    } catch (const std::exception& e) {
        traceLog("Warning: Exception scanning profiles directory: %s", e.what());
    } catch (...) {
        traceLog("Warning: Unknown exception scanning profiles directory");
    }
}

bool ProfileManager::saveProfileToDisk(const TasteProfile& profile) {
    if (profile.isBuiltIn) return false; // Never overwrite built-in profiles

    ensureProfilesDirectoryExists();
    auto dir = getProfilesDirectory();
    std::string cleanName = sanitizeFilename(profile.name);

    auto tmpFile = dir.getChildFile(cleanName + ".tmp");
    auto targetFile = dir.getChildFile(cleanName + ".json");

    std::string jsonStr = profile.toJsonString();
    if (!tmpFile.replaceWithText(juce::String(jsonStr))) {
        return false;
    }

    return tmpFile.moveFileTo(targetFile);
}

bool ProfileManager::deleteCustomProfile(uint32_t profileId) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    for (auto it = customProfiles.begin(); it != customProfiles.end(); ++it) {
        if (it->profileId == profileId) {
            auto dir = getProfilesDirectory();
            auto targetFile = dir.getChildFile(sanitizeFilename(it->name) + ".json");
            if (targetFile.exists()) targetFile.deleteFile();
            customProfiles.erase(it);
            if (activeProfileId == profileId) {
                activeProfileId = 1; // Fallback to Modern Worship
            }
            return true;
        }
    }
    return false;
}

std::vector<TasteProfile> ProfileManager::getAllProfiles() const {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    std::vector<TasteProfile> all = builtInProfiles;
    all.insert(all.end(), customProfiles.begin(), customProfiles.end());
    return all;
}

const TasteProfile* ProfileManager::getProfile(uint32_t profileId) const {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    for (const auto& p : builtInProfiles) {
        if (p.profileId == profileId) return &p;
    }
    for (const auto& p : customProfiles) {
        if (p.profileId == profileId) return &p;
    }
    return &builtInProfiles[0];
}

TasteProfile* ProfileManager::findActiveProfileMutable() {
    for (auto& p : customProfiles) {
        if (p.profileId == activeProfileId) return &p;
    }
    return nullptr; // Built-ins are immutable
}

void ProfileManager::setActiveProfile(uint32_t profileId) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    activeProfileId = profileId;
}

void ProfileManager::setFollowPercent(float follow) {
    followPercent = std::clamp(follow, 0.0f, 100.0f);
}

void ProfileManager::setMacroVocalForward(float v) {
    macroVocalForward = std::clamp(v, -100.0f, 100.0f);
}

void ProfileManager::setMacroEnergy(float v) {
    macroEnergy = std::clamp(v, -100.0f, 100.0f);
}

void ProfileManager::setMacroWarmBright(float v) {
    macroWarmBright = std::clamp(v, -100.0f, 100.0f);
}

void ProfileManager::setMacroSpace(float v) {
    macroSpace = std::clamp(v, -100.0f, 100.0f);
}

void ProfileManager::setMacroSaturation(float v) {
    macroSaturation = std::clamp(v, -100.0f, 100.0f);
}

void ProfileManager::setSessionMode(SessionMode mode) {
    currentSessionMode = mode;
}

void ProfileManager::setFxSettings(const FxSettings& fx) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    auto* p = findActiveProfileMutable();
    if (p) {
        p->fx = fx;
        saveProfileToDisk(*p);
    }
}

void ProfileManager::setCategoryReverbSend(FxCategory cat, float sendDb) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    auto* p = findActiveProfileMutable();
    if (p && static_cast<size_t>(cat) < p->reverbSendDb.size()) {
        p->reverbSendDb[static_cast<size_t>(cat)] = std::clamp(sendDb, -60.0f, 0.0f);
        saveProfileToDisk(*p);
    }
}

void ProfileManager::setCategoryDelaySend(FxCategory cat, float sendDb) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    auto* p = findActiveProfileMutable();
    if (p && static_cast<size_t>(cat) < p->delaySendDb.size()) {
        p->delaySendDb[static_cast<size_t>(cat)] = std::clamp(sendDb, -60.0f, 0.0f);
        saveProfileToDisk(*p);
    }
}

GlobalTasteState ProfileManager::buildGlobalTasteState() const {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    const TasteProfile* p = getProfile(activeProfileId);
    if (!p) p = &builtInProfiles[0];

    GlobalTasteState state;
    state.profileId = p->profileId;
    size_t nameLen = (std::min)(p->name.size(), sizeof(state.profileName) - 1);
    std::memcpy(state.profileName, p->name.data(), nameLen);
    state.profileName[nameLen] = '\0';

    state.followPercent = followPercent;
    state.macroVocalForward = macroVocalForward;
    state.macroEnergy = macroEnergy;
    state.macroWarmBright = macroWarmBright;
    state.macroSpace = macroSpace;
    state.macroSaturation = macroSaturation;
    state.sessionMode = currentSessionMode;

    state.reverbType = p->fx.reverbType;
    state.reverbDecaySec = p->fx.reverbDecaySec;
    state.reverbPredelayMs = p->fx.reverbPredelayMs;
    state.reverbDamping = p->fx.reverbDamping;

    state.delaySubdivision = p->fx.delaySubdivision;
    state.delayFeedback = p->fx.delayFeedback;
    state.delayPingPong = p->fx.delayPingPong;

    for (size_t i = 0; i < NUM_FX_CATEGORIES && i < p->reverbSendDb.size(); ++i) {
        state.reverbSendDb[i] = p->reverbSendDb[i];
        state.delaySendDb[i] = p->delaySendDb[i];
    }

    state.masterLowShelfDb = p->tonalDelta.lowShelfDb;
    state.masterPresenceDb = p->tonalDelta.presenceDb;
    state.masterAirShelfDb = p->tonalDelta.airShelfDb;

    state.maskingScale = p->maskingScale;
    state.warmthScale = p->warmthScale;
    state.transientScale = p->transientScale;
    state.widthScale = p->widthScale;

    for (size_t i = 0; i < NUM_INSTRUMENT_SLOTS && i < p->rideOffsetDelta.size(); ++i) {
        state.rideOffsetDelta[i] = p->rideOffsetDelta[i];
        state.characterTiltDb[i] = p->characterTiltDb[i];
    }

    return state;
}

bool ProfileManager::processProposal(const InstanceTelemetry& inst, uint64_t nowMs) {
    if (inst.proposalSequence == 0) return false;

    std::lock_guard<std::recursive_mutex> lock(mutex);

    // Sequence check: ignore repeated proposal packets
    auto seqIt = lastProcessedProposalSeq.find(inst.instanceId);
    if (seqIt != lastProcessedProposalSeq.end() && seqIt->second == inst.proposalSequence) {
        return false;
    }

    // Rate-limit check: at most once per instrument per 30 seconds
    uint16_t instTypeKey = static_cast<uint16_t>(inst.instrumentType);
    auto timeIt = lastInstrumentMergeMs.find(instTypeKey);
    if (timeIt != lastInstrumentMergeMs.end() && (nowMs < timeIt->second + 30000)) {
        return false;
    }

    // Find current active profile
    const TasteProfile* current = nullptr;
    for (const auto& bp : builtInProfiles) {
        if (bp.profileId == activeProfileId) { current = &bp; break; }
    }
    if (!current) {
        for (const auto& cp : customProfiles) {
            if (cp.profileId == activeProfileId) { current = &cp; break; }
        }
    }
    if (!current) current = &builtInProfiles[0];

    TasteProfile* targetProfile = nullptr;

    // Auto-fork built-in profile if active
    if (current->isBuiltIn) {
        std::string forkedName = "My " + current->name;
        TasteProfile* existingFork = nullptr;
        for (auto& cp : customProfiles) {
            if (cp.name == forkedName) {
                existingFork = &cp;
                break;
            }
        }

        if (existingFork) {
            activeProfileId = existingFork->profileId;
            targetProfile = existingFork;
        } else {
            TasteProfile forked = *current;
            forked.isBuiltIn = false;
            forked.forkSource = current->name;
            forked.name = forkedName;

            uint32_t maxId = 100;
            for (const auto& cp : customProfiles) {
                if (cp.profileId > maxId) maxId = cp.profileId;
            }
            forked.profileId = maxId + 1;

            customProfiles.push_back(forked);
            activeProfileId = forked.profileId;
            targetProfile = &customProfiles.back();
        }
    } else {
        targetProfile = findActiveProfileMutable();
    }

    if (!targetProfile) return false;

    // EMA Merge: newValue = 0.8 * old + 0.2 * proposed
    size_t slotIdx = static_cast<size_t>(inst.instrumentType);
    if (slotIdx < targetProfile->rideOffsetDelta.size()) {
        float oldRide = targetProfile->rideOffsetDelta[slotIdx];
        targetProfile->rideOffsetDelta[slotIdx] = 0.8f * oldRide + 0.2f * inst.proposalRideDelta;

        float oldTilt = targetProfile->characterTiltDb[slotIdx];
        targetProfile->characterTiltDb[slotIdx] = 0.8f * oldTilt + 0.2f * inst.proposalCharacterTiltDb;
    }

    targetProfile->warmthScale = std::clamp(0.8f * targetProfile->warmthScale + 0.2f * inst.proposalWarmthScale, 0.0f, 2.0f);

    FxCategory cat = TasteProfile::getCategoryForInstrument(inst.instrumentType);
    size_t catIdx = static_cast<size_t>(cat);
    if (catIdx < targetProfile->reverbSendDb.size() && targetProfile->reverbSendDb[catIdx] > -50.0f) {
        float oldSend = targetProfile->reverbSendDb[catIdx];
        float proposedSend = oldSend + inst.proposalSendTrimDb;
        targetProfile->reverbSendDb[catIdx] = std::clamp(0.8f * oldSend + 0.2f * proposedSend, -60.0f, 0.0f);
    }

    // Save forked or modified custom profile atomically to disk
    saveProfileToDisk(*targetProfile);

    // Record bookkeeping
    lastProcessedProposalSeq[inst.instanceId] = inst.proposalSequence;
    lastInstrumentMergeMs[instTypeKey] = nowMs;
    totalLearnedUpdates++;

    lastLearnedMessage = "Updated " + std::string(getInstrumentName(inst.instrumentType)) +
                         " (" + std::to_string(totalLearnedUpdates) + " total)";

    return true;
}

void ProfileManager::resetActiveProfileToDefault() {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    TasteProfile* current = findActiveProfileMutable();
    if (!current) return;

    // Find base profile to reset to
    const TasteProfile* base = nullptr;
    if (!current->forkSource.empty()) {
        for (const auto& bp : builtInProfiles) {
            if (bp.name == current->forkSource) {
                base = &bp;
                break;
            }
        }
    }
    if (!base) base = &builtInProfiles[0]; // fallback default

    std::string customName = current->name;
    uint32_t customId = current->profileId;
    std::string forkSrc = current->forkSource;

    *current = *base;
    current->isBuiltIn = false;
    current->name = customName;
    current->profileId = customId;
    current->forkSource = forkSrc;

    saveProfileToDisk(*current);
    totalLearnedUpdates = 0;
    lastLearnedMessage = "Reset to " + base->name + " defaults";
}

} // namespace aimixer
