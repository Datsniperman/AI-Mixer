#include "TasteProfile.h"

namespace aimixer {

TasteProfile::TasteProfile() {
    rideOffsetDelta.fill(0.0f);
    characterTiltDb.fill(0.0f);
    // Categories: Lead=0, Pastor=1, BgVocal=2, Snare=3, Keys=4, Guitars=5, Pad=6, Tracks=7
    reverbSendDb = { -12.0f, -100.0f, -9.0f, -14.0f, -13.0f, -15.0f, -10.0f, -16.0f };
    delaySendDb  = { -18.0f, -100.0f, -100.0f, -100.0f, -100.0f, -16.0f, -18.0f, -100.0f };
}

FxCategory TasteProfile::getCategoryForInstrument(InstrumentType type) noexcept {
    switch (type) {
        case InstrumentType::LeadVocal:      return FxCategory::Lead;
        case InstrumentType::PastorSpeaking: return FxCategory::Pastor;
        case InstrumentType::BackgroundVocal:
        case InstrumentType::Choir:          return FxCategory::BgVocal;
        case InstrumentType::Snare:          return FxCategory::Snare;
        case InstrumentType::Piano:
        case InstrumentType::Keys:           return FxCategory::Keys;
        case InstrumentType::AcousticGuitar:
        case InstrumentType::ElectricGuitar: return FxCategory::Guitars;
        case InstrumentType::Pad:
        case InstrumentType::Synth:          return FxCategory::Pad;
        case InstrumentType::Tracks:         return FxCategory::Tracks;
        case InstrumentType::Kick:
        case InstrumentType::Bass:
        case InstrumentType::Click:
            return FxCategory::Pastor; // Inactive / -inf sends by default
        case InstrumentType::Toms:
        case InstrumentType::HiHat:
        case InstrumentType::Overheads:
        case InstrumentType::Room:
            return FxCategory::Snare;
        case InstrumentType::Unknown:
        default:
            return FxCategory::Tracks;
    }
}

bool TasteProfile::isFxEnabledByDefault(InstrumentType type) noexcept {
    switch (type) {
        case InstrumentType::LeadVocal:
        case InstrumentType::BackgroundVocal:
        case InstrumentType::Choir:
        case InstrumentType::Snare:
        case InstrumentType::Piano:
        case InstrumentType::Keys:
        case InstrumentType::Pad:
        case InstrumentType::Synth:
        case InstrumentType::AcousticGuitar:
        case InstrumentType::ElectricGuitar:
        case InstrumentType::Tracks:
            return true;
        case InstrumentType::PastorSpeaking: // Intelligibility (always -inf)
        case InstrumentType::Kick:           // Always -inf
        case InstrumentType::Bass:           // Always -inf
        case InstrumentType::Toms:
        case InstrumentType::HiHat:
        case InstrumentType::Overheads:
        case InstrumentType::Room:
        case InstrumentType::Click:          // Always -inf
        case InstrumentType::Unknown:
        default:
            return false;
    }
}

TasteProfile TasteProfile::createModernWorship() {
    TasteProfile p;
    p.profileId = 1;
    p.name = "Modern Worship";
    p.isBuiltIn = true;
    p.forkSource = "";

    p.maskingScale = 1.0f;
    p.warmthScale = 1.0f;
    p.transientScale = 1.0f;
    p.widthScale = 1.0f;
    p.rideOffsetDelta.fill(0.0f);
    p.characterTiltDb.fill(0.0f);

    p.tonalDelta = { 0.0f, 0.0f, 0.0f };

    p.fx.reverbType = ReverbType::Hall;
    p.fx.reverbDecaySec = 3.2f;
    p.fx.reverbPredelayMs = 20.0f;
    p.fx.reverbDamping = 0.50f;

    p.fx.delaySubdivision = DelaySubdivision::DottedEighth;
    p.fx.delayFeedback = 0.35f;
    p.fx.delayPingPong = true;

    // Lead=0, Pastor=1, BgVocal=2, Snare=3, Keys=4, Guitars=5, Pad=6, Tracks=7
    p.reverbSendDb = { -12.0f, -100.0f, -9.0f, -14.0f, -13.0f, -15.0f, -10.0f, -16.0f };
    p.delaySendDb  = { -18.0f, -100.0f, -100.0f, -100.0f, -100.0f, -16.0f, -18.0f, -100.0f };
    return p;
}

TasteProfile TasteProfile::createAcousticWorship() {
    TasteProfile p;
    p.profileId = 2;
    p.name = "Acoustic Worship";
    p.isBuiltIn = true;
    p.forkSource = "";

    p.maskingScale = 0.8f;
    p.warmthScale = 1.2f;
    p.transientScale = 0.7f;
    p.widthScale = 0.9f;

    p.rideOffsetDelta.fill(0.0f);
    // Percussive categories -2 dB
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::Kick)]      = -2.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::Snare)]     = -2.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::Toms)]      = -2.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::HiHat)]     = -2.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::Overheads)] = -2.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::Room)]      = -2.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::Pad)]       = -3.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::LeadVocal)] = 0.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::AcousticGuitar)] = 2.0f;

    p.characterTiltDb.fill(0.0f);
    p.tonalDelta = { 0.0f, 0.0f, 0.5f }; // Tonal 0/0/+0.5

    p.fx.reverbType = ReverbType::Room;
    p.fx.reverbDecaySec = 1.6f;
    p.fx.reverbPredelayMs = 10.0f;
    p.fx.reverbDamping = 0.60f;

    p.fx.delaySubdivision = DelaySubdivision::Eighth;
    p.fx.delayFeedback = 0.25f;
    p.fx.delayPingPong = false;

    // Reverb = Modern Worship - 3 dB (Pastor stays -inf)
    p.reverbSendDb = { -15.0f, -100.0f, -12.0f, -17.0f, -16.0f, -18.0f, -13.0f, -19.0f };
    // Delay = default - 4 dB
    p.delaySendDb  = { -22.0f, -100.0f, -100.0f, -100.0f, -100.0f, -20.0f, -22.0f, -100.0f };
    return p;
}

TasteProfile TasteProfile::createGospel() {
    TasteProfile p;
    p.profileId = 3;
    p.name = "Gospel";
    p.isBuiltIn = true;
    p.forkSource = "";

    p.maskingScale = 1.1f;
    p.warmthScale = 1.4f;
    p.transientScale = 1.1f;
    p.widthScale = 1.1f;

    p.rideOffsetDelta.fill(0.0f);
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::BackgroundVocal)] = 2.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::Choir)]           = 2.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::Piano)]           = 2.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::Keys)]            = 2.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::Bass)]            = 1.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::LeadVocal)]       = 0.0f;

    p.characterTiltDb.fill(0.0f);
    p.tonalDelta = { 0.5f, 0.5f, 0.0f }; // Tonal +0.5/+0.5/0

    p.fx.reverbType = ReverbType::Plate;
    p.fx.reverbDecaySec = 2.4f;
    p.fx.reverbPredelayMs = 15.0f;
    p.fx.reverbDamping = 0.40f;

    p.fx.delaySubdivision = DelaySubdivision::DottedEighth;
    p.fx.delayFeedback = 0.40f;
    p.fx.delayPingPong = true;

    // Reverb = default + 2 dB (Pastor stays -inf)
    p.reverbSendDb = { -10.0f, -100.0f, -7.0f, -12.0f, -11.0f, -13.0f, -8.0f, -14.0f };
    // Delay = Lead -16, others default + 2 dB where not -inf
    p.delaySendDb  = { -16.0f, -100.0f, -100.0f, -100.0f, -100.0f, -14.0f, -16.0f, -100.0f };
    return p;
}

TasteProfile TasteProfile::createRockPop() {
    TasteProfile p;
    p.profileId = 4;
    p.name = "Rock / Pop";
    p.isBuiltIn = true;
    p.forkSource = "";

    p.maskingScale = 1.2f;
    p.warmthScale = 1.3f;
    p.transientScale = 1.4f;
    p.widthScale = 1.0f;

    p.rideOffsetDelta.fill(0.0f);
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::Kick)]            = 1.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::Snare)]           = 1.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::ElectricGuitar)]  = 2.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::BackgroundVocal)] = -1.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::Choir)]           = -1.0f;
    p.rideOffsetDelta[static_cast<size_t>(InstrumentType::Pad)]             = -3.0f;

    p.characterTiltDb.fill(0.0f);
    p.tonalDelta = { 0.5f, 1.0f, 0.5f }; // Tonal +0.5/+1.0/+0.5

    p.fx.reverbType = ReverbType::Plate;
    p.fx.reverbDecaySec = 1.8f;
    p.fx.reverbPredelayMs = 8.0f;
    p.fx.reverbDamping = 0.35f;

    p.fx.delaySubdivision = DelaySubdivision::Quarter;
    p.fx.delayFeedback = 0.30f;
    p.fx.delayPingPong = false;

    // Reverb: Lead -14, Snare -12, other non-Pastor default - 2 dB, Pastor -inf
    p.reverbSendDb = { -14.0f, -100.0f, -11.0f, -12.0f, -15.0f, -17.0f, -12.0f, -18.0f };
    // Delay: Guitars -14, Lead -20, rest -inf
    p.delaySendDb  = { -20.0f, -100.0f, -100.0f, -100.0f, -100.0f, -14.0f, -100.0f, -100.0f };
    return p;
}

std::vector<TasteProfile> TasteProfile::getBuiltInProfiles() {
    return {
        createModernWorship(),
        createAcousticWorship(),
        createGospel(),
        createRockPop()
    };
}

const TasteProfile* TasteProfile::getBuiltInProfile(uint32_t id) {
    static const auto profiles = getBuiltInProfiles();
    for (const auto& p : profiles) {
        if (p.profileId == id) return &p;
    }
    return &profiles[0]; // fallback default Modern Worship
}

juce::var TasteProfile::toVar() const {
    auto* obj = new juce::DynamicObject();
    obj->setProperty("profileId", static_cast<int>(profileId));
    obj->setProperty("name", juce::String(name));
    obj->setProperty("isBuiltIn", isBuiltIn);
    obj->setProperty("forkSource", juce::String(forkSource));

    obj->setProperty("maskingScale", maskingScale);
    obj->setProperty("warmthScale", warmthScale);
    obj->setProperty("transientScale", transientScale);
    obj->setProperty("widthScale", widthScale);

    juce::Array<juce::var> rideArray;
    for (size_t i = 0; i < rideOffsetDelta.size(); ++i) rideArray.add(rideOffsetDelta[i]);
    obj->setProperty("rideOffsetDelta", rideArray);

    juce::Array<juce::var> tiltArray;
    for (size_t i = 0; i < characterTiltDb.size(); ++i) tiltArray.add(characterTiltDb[i]);
    obj->setProperty("characterTiltDb", tiltArray);

    auto* tonalObj = new juce::DynamicObject();
    tonalObj->setProperty("lowShelfDb", tonalDelta.lowShelfDb);
    tonalObj->setProperty("presenceDb", tonalDelta.presenceDb);
    tonalObj->setProperty("airShelfDb", tonalDelta.airShelfDb);
    obj->setProperty("tonalDelta", tonalObj);

    auto* fxObj = new juce::DynamicObject();
    fxObj->setProperty("reverbType", static_cast<int>(fx.reverbType));
    fxObj->setProperty("reverbDecaySec", fx.reverbDecaySec);
    fxObj->setProperty("reverbPredelayMs", fx.reverbPredelayMs);
    fxObj->setProperty("reverbDamping", fx.reverbDamping);
    fxObj->setProperty("delaySubdivision", static_cast<int>(fx.delaySubdivision));
    fxObj->setProperty("delayFeedback", fx.delayFeedback);
    fxObj->setProperty("delayPingPong", fx.delayPingPong);
    obj->setProperty("fx", fxObj);

    juce::Array<juce::var> revSendArray;
    for (size_t i = 0; i < reverbSendDb.size(); ++i) revSendArray.add(reverbSendDb[i]);
    obj->setProperty("reverbSendDb", revSendArray);

    juce::Array<juce::var> dlySendArray;
    for (size_t i = 0; i < delaySendDb.size(); ++i) dlySendArray.add(delaySendDb[i]);
    obj->setProperty("delaySendDb", dlySendArray);

    return juce::var(obj);
}

std::optional<TasteProfile> TasteProfile::fromVar(const juce::var& v) {
    if (!v.isObject()) return std::nullopt;
    auto* obj = v.getDynamicObject();
    if (!obj) return std::nullopt;

    TasteProfile p;
    p.profileId = static_cast<uint32_t>(static_cast<int>(obj->getProperty("profileId")));
    p.name = obj->getProperty("name").toString().toStdString();
    p.isBuiltIn = static_cast<bool>(obj->getProperty("isBuiltIn"));
    p.forkSource = obj->getProperty("forkSource").toString().toStdString();

    p.maskingScale = static_cast<float>(obj->getProperty("maskingScale"));
    p.warmthScale = static_cast<float>(obj->getProperty("warmthScale"));
    p.transientScale = static_cast<float>(obj->getProperty("transientScale"));
    p.widthScale = static_cast<float>(obj->getProperty("widthScale"));

    if (auto* rArr = obj->getProperty("rideOffsetDelta").getArray()) {
        for (int i = 0; i < rArr->size() && i < static_cast<int>(p.rideOffsetDelta.size()); ++i) {
            p.rideOffsetDelta[i] = static_cast<float>((*rArr)[i]);
        }
    }

    if (auto* tArr = obj->getProperty("characterTiltDb").getArray()) {
        for (int i = 0; i < tArr->size() && i < static_cast<int>(p.characterTiltDb.size()); ++i) {
            p.characterTiltDb[i] = static_cast<float>((*tArr)[i]);
        }
    }

    if (auto* tObj = obj->getProperty("tonalDelta").getDynamicObject()) {
        p.tonalDelta.lowShelfDb = static_cast<float>(tObj->getProperty("lowShelfDb"));
        p.tonalDelta.presenceDb = static_cast<float>(tObj->getProperty("presenceDb"));
        p.tonalDelta.airShelfDb = static_cast<float>(tObj->getProperty("airShelfDb"));
    }

    if (auto* fObj = obj->getProperty("fx").getDynamicObject()) {
        p.fx.reverbType = static_cast<ReverbType>(static_cast<int>(fObj->getProperty("reverbType")));
        p.fx.reverbDecaySec = static_cast<float>(fObj->getProperty("reverbDecaySec"));
        p.fx.reverbPredelayMs = static_cast<float>(fObj->getProperty("reverbPredelayMs"));
        p.fx.reverbDamping = static_cast<float>(fObj->getProperty("reverbDamping"));
        p.fx.delaySubdivision = static_cast<DelaySubdivision>(static_cast<int>(fObj->getProperty("delaySubdivision")));
        p.fx.delayFeedback = static_cast<float>(fObj->getProperty("delayFeedback"));
        p.fx.delayPingPong = static_cast<bool>(fObj->getProperty("delayPingPong"));
    }

    if (auto* rsArr = obj->getProperty("reverbSendDb").getArray()) {
        for (int i = 0; i < rsArr->size() && i < static_cast<int>(p.reverbSendDb.size()); ++i) {
            p.reverbSendDb[i] = static_cast<float>((*rsArr)[i]);
        }
    }

    if (auto* dsArr = obj->getProperty("delaySendDb").getArray()) {
        for (int i = 0; i < dsArr->size() && i < static_cast<int>(p.delaySendDb.size()); ++i) {
            p.delaySendDb[i] = static_cast<float>((*dsArr)[i]);
        }
    }

    return p;
}

std::string TasteProfile::toJsonString() const {
    auto v = toVar();
    return juce::JSON::toString(v, false).toStdString();
}

std::optional<TasteProfile> TasteProfile::fromJsonString(const std::string& json) {
    auto parsed = juce::JSON::parse(juce::String(json));
    return fromVar(parsed);
}

} // namespace aimixer
