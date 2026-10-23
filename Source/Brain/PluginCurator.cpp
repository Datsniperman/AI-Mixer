#include "PluginCurator.h"
#include "../Common/LoadTrace.h"

namespace aimixer {

PluginCurator& PluginCurator::getInstance() {
    static PluginCurator instance;
    return instance;
}

PluginCurator::PluginCurator() {
    initializeCatalog();
}

bool PluginCurator::initializeCatalog() {
    std::lock_guard<std::mutex> lock(catalogMutex);

    juce::File appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                .getChildFile("AI Mixer");
    juce::File xmlFile = appDataDir.getChildFile("known-plugins.xml");

    if (!xmlFile.existsAsFile()) {
        traceLog("PluginCurator: known-plugins.xml not found at %s", xmlFile.getFullPathName().toRawUTF8());
        catalogLoaded = false;
        return false;
    }

    std::unique_ptr<juce::XmlElement> xml = juce::parseXML(xmlFile);
    if (xml == nullptr) {
        traceLog("PluginCurator: Failed to parse XML file: %s", xmlFile.getFullPathName().toRawUTF8());
        catalogLoaded = false;
        return false;
    }

    pluginList.recreateFromXml(*xml);
    catalogLoaded = true;
    traceLog("PluginCurator: Loaded %d plugins from known-plugins.xml", pluginList.getTypes().size());
    return true;
}

std::vector<juce::PluginDescription> PluginCurator::getAllWavesPlugins() const {
    std::lock_guard<std::mutex> lock(catalogMutex);
    std::vector<juce::PluginDescription> res;
    for (const auto& desc : pluginList.getTypes()) {
        if (desc.manufacturerName.containsIgnoreCase("Waves") ||
            desc.fileOrIdentifier.containsIgnoreCase("WaveShell")) {
            res.push_back(desc);
        }
    }
    return res;
}

const juce::PluginDescription* PluginCurator::findPlugin(const juce::String& namePattern,
                                                         bool preferStereo,
                                                         bool zeroLatencyOnly) const {
    std::lock_guard<std::mutex> lock(catalogMutex);
    const juce::PluginDescription* bestMatch = nullptr;

    for (const auto& desc : pluginList.getTypes()) {
        bool matchesName = desc.name.containsIgnoreCase(namePattern);
        if (!matchesName) continue;

        bool isWaves = desc.manufacturerName.containsIgnoreCase("Waves") ||
                       desc.fileOrIdentifier.containsIgnoreCase("WaveShell");
        if (!isWaves) continue;

        if (preferStereo && desc.name.containsIgnoreCase("Stereo") && !desc.name.containsIgnoreCase("Mono/Stereo")) {
            return &desc; // Exact match on native stereo
        }

        if (bestMatch == nullptr) {
            bestMatch = &desc;
        }
    }

    return bestMatch;
}

CuratedChainRecipe PluginCurator::getRecipeForSource(InstrumentType instrument) {
    CuratedChainRecipe recipe;

    switch (instrument) {
        case InstrumentType::LeadVocal:
            // Nashville 4-Stage Pro Vocal:
            // 1. Surgical Tone & Pre-EQ: SSLChannel (HPF 100Hz, carve 300Hz mud, high air)
            // 2. Dynamic De-Esser: RDeEsser (tames harsh sibilance before compression)
            // 3. Fast Transient Peak Catcher: CLA-76 (fast FET 4:1 taming 2-3 dB spikes)
            // 4. Warm Opto Leveler / Presence: CLA-2A / RVox (pins vocal upfront)
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "SSLChannel", "Scheps 73", "SSLEQ" });
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "RDeEsser", "DeEsser", nullptr });
            recipe.slots.push_back({ ChainSlotRole::FastPeakTamer, "CLA-76", "SSLChannel", nullptr });
            recipe.slots.push_back({ ChainSlotRole::OptoLeveler, "CLA-2A", "RVox", "Renaissance Vox" });
            break;

        case InstrumentType::PastorSpeaking:
            // Speech Intelligibility & Leveling:
            // 1. Pre-EQ & Gate: SSLChannel (HPF 120Hz, voice clarity)
            // 2. De-Esser: RDeEsser
            // 3. Upfront Leveler: RVox (compression + speech gating)
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "SSLChannel", "Scheps 73", nullptr });
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "RDeEsser", "DeEsser", nullptr });
            recipe.slots.push_back({ ChainSlotRole::OptoLeveler, "RVox", "CLA-2A", "Renaissance Vox" });
            break;

        case InstrumentType::BackgroundVocal:
        case InstrumentType::Choir:
            // BGV Wall: Scheps 73 (12kHz air sheen) -> RDeEsser -> CLA-2A smooth glue
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "Scheps 73", "SSLChannel", nullptr });
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "RDeEsser", "DeEsser", nullptr });
            recipe.slots.push_back({ ChainSlotRole::OptoLeveler, "CLA-2A", "RVox", "Renaissance Vox" });
            break;

        case InstrumentType::Kick:
            // Punch & Weight:
            // 1. Console Pre & 4-Band EQ: SSLChannel (HPF 35Hz, 60Hz thump, 350Hz scoop, 4.2kHz beater)
            // 2. Subharmonic Generator: RBass (tuned 60-80Hz sub foundation)
            // 3. Fast Punch Limiter: CLA-76 (FET snap)
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "SSLChannel", "SSL EV2", "API-550A" });
            recipe.slots.push_back({ ChainSlotRole::CharacterColor, "RBass", "Renaissance Bass", nullptr });
            recipe.slots.push_back({ ChainSlotRole::FastPeakTamer, "CLA-76", "SSLComp", nullptr });
            break;

        case InstrumentType::Snare:
            // Punch, Body & Sizzle:
            // 1. Console Channel: SSLChannel (HPF 80Hz, 200Hz body punch, 3.5kHz crack, noise gate)
            // 2. Fast FET Slammer: CLA-76 (all-buttons or fast 4:1 bringing out rattle)
            // 3. Warmth / Tape: Kramer Tape (head bump & analog transient rounding)
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "SSLChannel", "API-550A", "Scheps 73" });
            recipe.slots.push_back({ ChainSlotRole::FastPeakTamer, "CLA-76", "SSLComp", nullptr });
            recipe.slots.push_back({ ChainSlotRole::CharacterColor, "Kramer Tape", "Scheps 73", nullptr });
            break;

        case InstrumentType::Toms:
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "SSLChannel", "API-560", nullptr });
            recipe.slots.push_back({ ChainSlotRole::FastPeakTamer, "CLA-76", "SSLComp", nullptr });
            break;

        case InstrumentType::HiHat:
        case InstrumentType::Overheads:
        case InstrumentType::Room:
            // Cymbals & Rooms: Neve 1073 top sheen -> Opto leveler
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "Scheps 73", "SSLChannel", "API-550A" });
            recipe.slots.push_back({ ChainSlotRole::OptoLeveler, "CLA-2A", "CLA-3A", nullptr });
            break;

        case InstrumentType::Bass:
            // The Pinned Bass Trilogy:
            // 1. Fundamental Sub: RBass (sub harmonics for phone & small speaker reproduction)
            // 2. Tonal Definition: SSLChannel (100Hz warmth, 800Hz bite, 30Hz rumble cut)
            // 3. Fast Peak Clamp: CLA-76 (pins the pick attack)
            // 4. Opto Body Sustain: CLA-2A (smooth low-end glue)
            recipe.slots.push_back({ ChainSlotRole::CharacterColor, "RBass", "Renaissance Bass", nullptr });
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "SSLChannel", "API-550A", nullptr });
            recipe.slots.push_back({ ChainSlotRole::FastPeakTamer, "CLA-76", "SSLChannel", nullptr });
            recipe.slots.push_back({ ChainSlotRole::OptoLeveler, "CLA-2A", "CLA-3A", nullptr });
            break;

        case InstrumentType::AcousticGuitar:
            // Acoustic Sweetening:
            // 1. Air & De-Boom EQ: Scheps 73 (carve 250Hz boom, +3dB 12kHz sparkle)
            // 2. Optical Dynamics: Renaissance Axx / CLA-2A (smooth level without pumping)
            // 3. Tape Depth: Kramer Tape
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "Scheps 73", "API-550A", "SSLChannel" });
            recipe.slots.push_back({ ChainSlotRole::OptoLeveler, "Renaissance Axx", "CLA-2A", "CLA-3A" });
            recipe.slots.push_back({ ChainSlotRole::CharacterColor, "Kramer Tape", "Scheps 73", nullptr });
            break;

        case InstrumentType::ElectricGuitar:
            // Electric Bite & Sustain:
            // 1. Channel EQ: SSLChannel (HPF 90Hz, 2.5kHz bite, carve 500Hz mud)
            // 2. FET Mid Punch: CLA-76 (fast punch)
            // 3. Tape Saturation: Kramer Tape (tube/tape warmth)
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "SSLChannel", "Scheps 73", "API-550A" });
            recipe.slots.push_back({ ChainSlotRole::FastPeakTamer, "CLA-76", "CLA-3A", nullptr });
            recipe.slots.push_back({ ChainSlotRole::CharacterColor, "Kramer Tape", "Scheps 73", nullptr });
            break;

        case InstrumentType::Piano:
        case InstrumentType::Keys:
            // Grand Piano / Keys:
            // 1. Tonal Sweetener: Scheps 73 (Neve warm musical curve)
            // 2. Opto Leveler: CLA-2A (preserves natural hammer transients while sustaining decays)
            // 3. Tape Excitement: Kramer Tape
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "Scheps 73", "SSLChannel", "API-550A" });
            recipe.slots.push_back({ ChainSlotRole::OptoLeveler, "CLA-2A", "CLA-3A", "PuigChild 660" });
            recipe.slots.push_back({ ChainSlotRole::CharacterColor, "Kramer Tape", nullptr, nullptr });
            break;

        case InstrumentType::Pad:
        case InstrumentType::Synth:
        case InstrumentType::Tracks:
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "Scheps 73", "SSLChannel", nullptr });
            recipe.slots.push_back({ ChainSlotRole::OptoLeveler, "CLA-2A", "Renaissance Axx", nullptr });
            recipe.slots.push_back({ ChainSlotRole::CharacterColor, "Kramer Tape", nullptr, nullptr });
            break;

        case InstrumentType::Click:
            // Strict bit-exact bypass for click track
            break;

        case InstrumentType::Unknown:
        default:
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "SSLChannel", "Scheps 73", nullptr });
            recipe.slots.push_back({ ChainSlotRole::FastPeakTamer, "CLA-76", "CLA-2A", nullptr });
            recipe.slots.push_back({ ChainSlotRole::OptoLeveler, "CLA-2A", "RVox", nullptr });
            break;
    }

    return recipe;
}

CuratedChainRecipe PluginCurator::getRecipeForBus(BusType busType) {
    CuratedChainRecipe recipe;

    switch (busType) {
        case BusType::Drums:
            // The New York Drum Bus Masterpiece:
            // 1. Tonal EQ: API-550A or Scheps 73 (top sheen & tight kick weight)
            // 2. VCA Mix Bus Glue: SSLComp / SSL G-Master Bus Compressor (30ms attack, auto release, 2:1 glue)
            // 3. Tape Harmonics: Kramer Tape
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "API-550A", "Scheps 73", "SSLEQ" });
            recipe.slots.push_back({ ChainSlotRole::BusVcaGlue, "SSLComp", "API-2500", "SSLChannel" });
            recipe.slots.push_back({ ChainSlotRole::CharacterColor, "Kramer Tape", nullptr, nullptr });
            break;

        case BusType::Vocals:
            // Vocal Subgroup: Neve 1073 air -> Smooth Opto Glue
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "Scheps 73", "SSLChannel", nullptr });
            recipe.slots.push_back({ ChainSlotRole::BusVcaGlue, "CLA-2A", "SSLComp", "API-2500" });
            recipe.slots.push_back({ ChainSlotRole::CharacterColor, "Kramer Tape", nullptr, nullptr });
            break;

        case BusType::Music:
        case BusType::Generic:
        default:
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "Scheps 73", "API-550A", nullptr });
            recipe.slots.push_back({ ChainSlotRole::BusVcaGlue, "SSLComp", "API-2500", "CLA-2A" });
            break;

        case BusType::FX:
            recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "SSLEQ", "Scheps 73", nullptr });
            break;
    }

    return recipe;
}

CuratedChainRecipe PluginCurator::getRecipeForMaster() {
    CuratedChainRecipe recipe;
    // Mastering Chain:
    // 1. Master Tone & Harmonic Curve: Scheps 73 / API-550A
    // 2. Master VCA Bus Glue: SSLComp (2:1, 30ms, auto release, 1.5 dB glue)
    // 3. Master Tape Cohesion: Kramer Tape (15 IPS)
    // 4. Precision Brickwall Peak Limiter: L2 Ultramaximizer (-0.8 dBTP ceiling)
    recipe.slots.push_back({ ChainSlotRole::CleanAndTone, "Scheps 73", "API-550A", nullptr });
    recipe.slots.push_back({ ChainSlotRole::BusVcaGlue, "SSLComp", "API-2500", nullptr });
    recipe.slots.push_back({ ChainSlotRole::CharacterColor, "Kramer Tape", nullptr, nullptr });
    recipe.slots.push_back({ ChainSlotRole::TruePeakLimiter, "L2", "L3-LL", "L3" });
    return recipe;
}

std::vector<juce::PluginDescription> PluginCurator::resolveChain(const CuratedChainRecipe& recipe,
                                                                 bool isLiveMode,
                                                                 bool isStereo) const {
    std::vector<juce::PluginDescription> result;

    for (const auto& slot : recipe.slots) {
        const juce::PluginDescription* match = nullptr;

        // Try preferred plugin
        if (slot.preferredPlugin != nullptr) {
            match = findPlugin(slot.preferredPlugin, isStereo, isLiveMode);
        }

        // Try fallback plugin
        if (match == nullptr && slot.fallbackPlugin != nullptr) {
            match = findPlugin(slot.fallbackPlugin, isStereo, isLiveMode);
        }

        // Try secondary fallback
        if (match == nullptr && slot.secondaryFallback != nullptr) {
            match = findPlugin(slot.secondaryFallback, isStereo, isLiveMode);
        }

        if (match != nullptr) {
            // Avoid adding identical plugin multiple times in the same chain unless distinct roles
            bool alreadyIn = false;
            for (const auto& existing : result) {
                if (existing.name == match->name) {
                    alreadyIn = true;
                    break;
                }
            }
            if (!alreadyIn || match->name.containsIgnoreCase("SSLChannel") || match->name.containsIgnoreCase("CLA")) {
                result.push_back(*match);
            }
        }
    }

    return result;
}

} // namespace aimixer
