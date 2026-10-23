#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Common/Types.h"
#include <vector>
#include <string>
#include <memory>
#include <mutex>

namespace aimixer {

// ==============================================================================
// Slot archetype in a curated multi-plugin chain
// ==============================================================================
enum class ChainSlotRole : uint8_t {
    CleanAndTone = 0,    // Pre-EQ / Gate / Channel Strip (e.g. SSLChannel, Scheps 73)
    FastPeakTamer = 1,   // Fast FET compressor (e.g. CLA-76)
    OptoLeveler = 2,     // Smooth opto leveler / vocal compressor (e.g. CLA-2A, RVox)
    CharacterColor = 3,  // Harmonic color, sub harmonics, tape (e.g. RBass, Kramer Tape)
    BusVcaGlue = 4,      // Bus glue compressor (e.g. SSL G-Master, API-2500)
    TruePeakLimiter = 5  // Brickwall limiter (e.g. L2, L3-LL)
};

struct CuratedSlotTarget {
    ChainSlotRole slotRole = ChainSlotRole::CleanAndTone;
    const char* preferredPlugin = nullptr;
    const char* fallbackPlugin = nullptr;
    const char* secondaryFallback = nullptr;
};

struct CuratedChainRecipe {
    std::vector<CuratedSlotTarget> slots;
};

// ==============================================================================
// Plugin Curator Engine
// ==============================================================================
class PluginCurator {
public:
    static PluginCurator& getInstance();

    // Loads %APPDATA%/AI Mixer/known-plugins.xml
    bool initializeCatalog();
    bool isCatalogLoaded() const noexcept { return catalogLoaded; }
    size_t getDiscoveredPluginCount() const noexcept { return pluginList.getTypes().size(); }

    const juce::KnownPluginList& getPluginList() const noexcept { return pluginList; }
    std::vector<juce::PluginDescription> getAllWavesPlugins() const;

    // Search helper
    const juce::PluginDescription* findPlugin(const juce::String& namePattern,
                                              bool preferStereo = true,
                                              bool zeroLatencyOnly = false) const;

    // Recipes
    static CuratedChainRecipe getRecipeForSource(InstrumentType instrument);
    static CuratedChainRecipe getRecipeForBus(BusType busType);
    static CuratedChainRecipe getRecipeForMaster();

    // Build actual descriptions from recipe
    std::vector<juce::PluginDescription> resolveChain(const CuratedChainRecipe& recipe,
                                                      bool isLiveMode = false,
                                                      bool isStereo = true) const;

private:
    PluginCurator();
    ~PluginCurator() = default;

    juce::KnownPluginList pluginList;
    bool catalogLoaded = false;
    mutable std::mutex catalogMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginCurator)
};

} // namespace aimixer
