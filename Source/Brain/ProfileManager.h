#pragma once

#include "TasteProfile.h"
#include "../Common/SharedProtocol.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>

namespace aimixer {

class ProfileManager {
public:
    ProfileManager();
    ~ProfileManager() = default;

    // Profile Management & File I/O
    void scanProfilesDirectory();
    bool saveProfileToDisk(const TasteProfile& profile);
    bool deleteCustomProfile(uint32_t profileId);

    std::vector<TasteProfile> getAllProfiles() const;
    const TasteProfile* getProfile(uint32_t profileId) const;

    // Active Selection & Macro State
    void setActiveProfile(uint32_t profileId);
    void setFollowPercent(float follow);
    void setMacroVocalForward(float v);
    void setMacroEnergy(float v);
    void setMacroWarmBright(float v);
    void setMacroSpace(float v);
    void setMacroSaturation(float v);
    void setSessionMode(SessionMode mode);

    // Reverb / Delay direct coordinator adjustments
    void setFxSettings(const FxSettings& fx);
    void setCategoryReverbSend(FxCategory cat, float sendDb);
    void setCategoryDelaySend(FxCategory cat, float sendDb);

    // Coordinator Taste State broadcast generator
    GlobalTasteState buildGlobalTasteState() const;

    // Profile Learning (EMA on proposals)
    // Returns true if a proposal was accepted and merged
    bool processProposal(const InstanceTelemetry& inst, uint64_t nowMs);

    // Resets custom forked profile back to base built-in defaults
    void resetActiveProfileToDefault();

    int getLearnedUpdatesCount() const noexcept { return totalLearnedUpdates; }
    std::string getLastLearnedMessage() const { return lastLearnedMessage; }

    static juce::File getProfilesDirectory();

private:
    void ensureProfilesDirectoryExists();
    TasteProfile* findActiveProfileMutable();

    std::vector<TasteProfile> builtInProfiles;
    std::vector<TasteProfile> customProfiles;
    uint32_t activeProfileId = 1; // Default: Modern Worship

    // Broadcast macro & session state
    float followPercent = 100.0f;
    float macroVocalForward = 0.0f;
    float macroEnergy = 0.0f;
    float macroWarmBright = 0.0f;
    float macroSpace = 0.0f;
    float macroSaturation = 0.0f;
    SessionMode currentSessionMode = SessionMode::Live;

    // Rate-limiting map: instrumentType -> timestampMs of last merge
    std::unordered_map<uint16_t, uint64_t> lastInstrumentMergeMs;
    // Track sequence numbers per instanceId to only process new proposals
    std::unordered_map<uint32_t, uint32_t> lastProcessedProposalSeq;

    int totalLearnedUpdates = 0;
    std::string lastLearnedMessage = "No learned updates yet";
    mutable std::recursive_mutex mutex;
};

} // namespace aimixer
