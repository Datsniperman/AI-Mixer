#pragma once

#include "../Common/Types.h"
#include "../Common/SharedProtocol.h"
#include <juce_core/juce_core.h>
#include <string>
#include <vector>
#include <array>
#include <cmath>

namespace aimixer {

struct TonalDelta {
    float lowShelfDb = 0.0f;
    float presenceDb = 0.0f;
    float airShelfDb = 0.0f;
};

struct FxSettings {
    ReverbType reverbType = ReverbType::Hall;
    float reverbDecaySec = 3.2f;     // 0.4 - 6.0 sec
    float reverbPredelayMs = 20.0f;   // 0 - 40 ms
    float reverbDamping = 0.5f;       // 0 - 1
    DelaySubdivision delaySubdivision = DelaySubdivision::DottedEighth;
    float delayFeedback = 0.35f;      // 0 - 0.6
    bool delayPingPong = true;
};

class TasteProfile {
public:
    uint32_t profileId = 1;
    std::string name = "Modern Worship";
    bool isBuiltIn = true;
    std::string forkSource = "";

    // Solver & Playbook Deltas / Scalars
    std::array<float, 21> rideOffsetDelta{}; // per InstrumentType
    float maskingScale = 1.0f;               // 0.5 .. 1.5
    float warmthScale = 1.0f;                // 0.0 .. 2.0
    float transientScale = 1.0f;             // 0.0 .. 2.0
    float widthScale = 1.0f;                 // 0.5 .. 1.5

    // Per-instrument character tilt (high-shelf gain @ 8 kHz, -2 to +2 dB)
    std::array<float, 21> characterTiltDb{};

    // Master Tonal Target Delta
    TonalDelta tonalDelta;

    // Shared FX Character & Sends
    FxSettings fx;
    std::array<float, 8> reverbSendDb{}; // per FxCategory
    std::array<float, 8> delaySendDb{};  // per FxCategory

    TasteProfile();

    // Built-in Profiles
    static TasteProfile createModernWorship();
    static TasteProfile createAcousticWorship();
    static TasteProfile createGospel();
    static TasteProfile createRockPop();

    static std::vector<TasteProfile> getBuiltInProfiles();
    static const TasteProfile* getBuiltInProfile(uint32_t id);

    // JSON Serialisation & Parsing
    juce::var toVar() const;
    static std::optional<TasteProfile> fromVar(const juce::var& v);

    std::string toJsonString() const;
    static std::optional<TasteProfile> fromJsonString(const std::string& json);

    // Helpers
    static FxCategory getCategoryForInstrument(InstrumentType type) noexcept;
    static bool isFxEnabledByDefault(InstrumentType type) noexcept;
};

} // namespace aimixer
