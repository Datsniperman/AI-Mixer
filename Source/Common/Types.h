#pragma once

#include <cstdint>
#include <string_view>

namespace aimixer {

// ==============================================================================
// Plugin Roles
// ==============================================================================
enum class PluginRole : uint8_t {
    Source = 0,
    Bus = 1,
    Master = 2
};

inline const char* getRoleName(PluginRole role) noexcept {
    switch (role) {
        case PluginRole::Source: return "Source";
        case PluginRole::Bus:    return "Bus";
        case PluginRole::Master: return "Master";
        default:                 return "Unknown";
    }
}

// ==============================================================================
// Side Hint (L / R channel suffix tracking)
// ==============================================================================
enum class SideHint : uint8_t {
    Centre = 0,
    Left = 1,
    Right = 2
};

inline const char* getSideHintName(SideHint side) noexcept {
    switch (side) {
        case SideHint::Left:   return "Left";
        case SideHint::Right:  return "Right";
        case SideHint::Centre:
        default:               return "Centre";
    }
}

// ==============================================================================
// Bus Types (Bus Role sub-classification)
// ==============================================================================
enum class BusType : uint8_t {
    Generic = 0,
    Drums,
    Vocals,
    Music,
    FX
};

inline const char* getBusTypeName(BusType type) noexcept {
    switch (type) {
        case BusType::Drums:   return "Drums Bus";
        case BusType::Vocals:  return "Vocals Bus";
        case BusType::Music:   return "Music Bus";
        case BusType::FX:      return "FX Bus";
        case BusType::Generic:
        default:               return "Generic Bus";
    }
}

// ==============================================================================
// Identity Source (Layer tracking for UI & Precedence)
// ==============================================================================
enum class IdentitySource : uint8_t {
    None = 0,
    TrackName = 1,
    User = 2,
    Classified = 3,
    Neighbour = 4
};

inline const char* getIdentitySourceName(IdentitySource src) noexcept {
    switch (src) {
        case IdentitySource::User:       return "Manual User Pick";
        case IdentitySource::TrackName:  return "Track Name Matched";
        case IdentitySource::Classified: return "Audio Feature Analysis";
        case IdentitySource::Neighbour:  return "Session Context";
        case IdentitySource::None:
        default:                         return "Default / Unknown";
    }
}

// ==============================================================================
// Instrument Types (19 categories)
// ==============================================================================
enum class InstrumentType : uint16_t {
    Unknown = 0,
    LeadVocal,
    PastorSpeaking,
    BackgroundVocal,
    Choir,
    Kick,
    Snare,
    Toms,
    HiHat,
    Overheads,
    Bass,
    AcousticGuitar,
    ElectricGuitar,
    Piano,
    Keys,
    Pad,
    Synth,
    Tracks,
    Click,
    Room
};

inline const char* getInstrumentName(InstrumentType type) noexcept {
    switch (type) {
        case InstrumentType::LeadVocal:       return "Lead Vocal";
        case InstrumentType::PastorSpeaking:  return "Pastor / Speech";
        case InstrumentType::BackgroundVocal: return "BGV";
        case InstrumentType::Choir:           return "Choir";
        case InstrumentType::Kick:            return "Kick";
        case InstrumentType::Snare:           return "Snare";
        case InstrumentType::Toms:            return "Toms";
        case InstrumentType::HiHat:           return "Hi-Hat";
        case InstrumentType::Overheads:       return "Overheads";
        case InstrumentType::Bass:            return "Bass";
        case InstrumentType::AcousticGuitar:  return "Acoustic Gtr";
        case InstrumentType::ElectricGuitar:  return "Electric Gtr";
        case InstrumentType::Piano:           return "Piano";
        case InstrumentType::Keys:            return "Keys / Organ";
        case InstrumentType::Pad:             return "Pad / Ambient";
        case InstrumentType::Synth:           return "Synth Lead/Arp";
        case InstrumentType::Tracks:          return "Tracks / Playback";
        case InstrumentType::Click:           return "Click (Pass-Through)";
        case InstrumentType::Room:            return "Room / Crowd";
        case InstrumentType::Unknown:
        default:                              return "Generic / Unknown";
    }
}

// ==============================================================================
// Master Destination Tag (v2 PA + Broadcast architecture ready)
// ==============================================================================
enum class DestinationTag : uint8_t {
    PALive = 0,       // Headroom + intelligibility, -1 dBTP
    Broadcast = 1,    // -14 to -16 LUFS-I, speech clarity forward
    Studio = 2        // -14 LUFS-I user target
};

inline const char* getDestinationName(DestinationTag dest) noexcept {
    switch (dest) {
        case DestinationTag::Broadcast: return "Broadcast";
        case DestinationTag::Studio:    return "Studio";
        case DestinationTag::PALive:
        default:                        return "PA Live";
    }
}

// ==============================================================================
// Pan / Space object - Designed with future immersive coordinates {azimuth, width}
// ==============================================================================
struct PanPosition {
    float azimuth = 0.0f; // -1.0f (Full Left) to +1.0f (Full Right); later 0-360 deg
    float width   = 1.0f; // 0.0f (Mono) to 1.0f (Full Stereo) / >1.0f (Super-wide)
};

// ==============================================================================
// Reason Codes for Directives (Target instance formats plain English locally)
// ==============================================================================
enum class ReasonCode : uint8_t {
    None = 0,
    MaskingVocal,
    MaskingKick,
    MaskingBass,
    MaskingSnare,
    MaskingPiano,
    MaskingGeneric,
    RideTarget,
    RideSoloPromote,
    DuckToKick,
    StagingHot,
    StagingQuiet
};

inline const char* getReasonDescription(ReasonCode code) noexcept {
    switch (code) {
        case ReasonCode::MaskingVocal:    return "masking Lead Vocal";
        case ReasonCode::MaskingKick:     return "masking Kick";
        case ReasonCode::MaskingBass:     return "masking Bass";
        case ReasonCode::MaskingSnare:    return "masking Snare";
        case ReasonCode::MaskingPiano:    return "masking Piano";
        case ReasonCode::MaskingGeneric:  return "masking collision";
        case ReasonCode::RideTarget:      return "target relative to vocal";
        case ReasonCode::RideSoloPromote: return "solo promotion reference";
        case ReasonCode::DuckToKick:      return "ducking to Kick";
        case ReasonCode::StagingHot:      return "peak headroom guard";
        case ReasonCode::StagingQuiet:    return "buried level lift";
        case ReasonCode::None:
        default:                          return "mix directive";
    }
}

// ==============================================================================
// Masking Cut Directive (Negative cuts only: [-6 dB, 0 dB])
// ==============================================================================
struct MaskingCutDirective {
    bool enabled = false;
    float freqHz = 1000.0f;
    float gainDb = 0.0f;    // NEGATIVE ONLY: clamped [-6.0f, 0.0f]
    float q = 1.4f;
    ReasonCode reason = ReasonCode::None;
    uint32_t otherInstanceId = 0;
    InstrumentType otherInstrument = InstrumentType::Unknown;
};

// ==============================================================================
// Manual Takeover Flags
// ==============================================================================
namespace ManualOverride {
    constexpr uint32_t None      = 0;
    constexpr uint32_t Trim      = 1 << 0;
    constexpr uint32_t HPF       = 1 << 1;
    constexpr uint32_t LPF       = 1 << 2;
    constexpr uint32_t Comp      = 1 << 3;
    constexpr uint32_t LevelRide = 1 << 4;
    constexpr uint32_t PanWidth  = 1 << 5;
    constexpr uint32_t Freeze    = 1 << 6;
    constexpr uint32_t EQ        = 1 << 7;
    constexpr uint32_t Gate      = 1 << 8;
    constexpr uint32_t Warmth    = 1 << 9;
    constexpr uint32_t Character = 1 << 10;
    constexpr uint32_t DeEss     = 1 << 11;
    constexpr uint32_t Transient = 1 << 12;
    constexpr uint32_t ReverbSend = 1 << 13;
    constexpr uint32_t DelaySend  = 1 << 14;
}

// ==============================================================================
// Session Modes (MVP 5)
// ==============================================================================
enum class SessionMode : uint8_t {
    Live = 0,       // Standard live performance (safe slew rates, 5 Hz solver)
    Learn = 1,      // Soundcheck / rehearsal (faster slew rates, deeper caps, 10 Hz)
    FreezeAll = 2   // Lock current mix static across all instances
};

inline const char* getSessionModeName(SessionMode mode) noexcept {
    switch (mode) {
        case SessionMode::Learn:     return "Learn";
        case SessionMode::FreezeAll: return "Freeze All";
        case SessionMode::Live:
        default:                     return "Live";
    }
}

// ==============================================================================
// Shared FX Types (MVP 5)
// ==============================================================================
enum class ReverbType : uint8_t {
    Hall = 0,
    Plate = 1,
    Room = 2
};

inline const char* getReverbTypeName(ReverbType type) noexcept {
    switch (type) {
        case ReverbType::Plate: return "Plate";
        case ReverbType::Room:  return "Room";
        case ReverbType::Hall:
        default:                return "Hall";
    }
}

enum class DelaySubdivision : uint8_t {
    DottedEighth = 0,
    Quarter = 1,
    Eighth = 2
};

inline const char* getDelaySubdivisionName(DelaySubdivision sub) noexcept {
    switch (sub) {
        case DelaySubdivision::Quarter: return "1/4 Note";
        case DelaySubdivision::Eighth:  return "1/8 Note";
        case DelaySubdivision::DottedEighth:
        default:                        return "Dotted 1/8";
    }
}

enum class FxCategory : uint8_t {
    Lead = 0,       // Lead Vocal
    Pastor = 1,     // Pastor Speaking
    BgVocal = 2,    // Background Vocal & Choir
    Snare = 3,      // Snare / Percussion
    Keys = 4,       // Keys / Piano
    Guitars = 5,    // Acoustic & Electric Guitars
    Pad = 6,        // Pad / Ambient / Synth
    Tracks = 7      // Tracks / Playback / Other
};

inline const char* getFxCategoryName(FxCategory cat) noexcept {
    switch (cat) {
        case FxCategory::Lead:    return "Lead";
        case FxCategory::Pastor:  return "Pastor";
        case FxCategory::BgVocal: return "BGV";
        case FxCategory::Snare:   return "Snare";
        case FxCategory::Keys:    return "Keys/Piano";
        case FxCategory::Guitars: return "Guitars";
        case FxCategory::Pad:     return "Pad/Synth";
        case FxCategory::Tracks:
        default:                  return "Tracks";
    }
}

} // namespace aimixer
