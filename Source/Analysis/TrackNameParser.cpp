#include "TrackNameParser.h"
#include <algorithm>
#include <cctype>
#include <vector>

namespace aimixer {

namespace {

struct AliasEntry {
    std::string_view alias;
    InstrumentType type;
    float confidence;
};

// Aliases ordered by specificity and longest match first
const AliasEntry ALIAS_DICTIONARY[] = {
    // Drums - Overheads
    { "drum overheads",   InstrumentType::Overheads,       0.95f },
    { "drum overhead",    InstrumentType::Overheads,       0.95f },
    { "drum oh",          InstrumentType::Overheads,       0.95f },
    { "kit oh",           InstrumentType::Overheads,       0.95f },
    { "overheads",        InstrumentType::Overheads,       0.95f },
    { "overhead",         InstrumentType::Overheads,       0.95f },
    { "cymbals",          InstrumentType::Overheads,       0.90f },
    { "cymbal",           InstrumentType::Overheads,       0.90f },
    { "oh l",             InstrumentType::Overheads,       0.95f },
    { "oh r",             InstrumentType::Overheads,       0.95f },
    { "oh",               InstrumentType::Overheads,       0.90f },

    // Room / Ambient mics (Fix: room drums, drum room, room mic -> Room)
    { "room drums",       InstrumentType::Room,            0.90f },
    { "ambient mic",      InstrumentType::Room,            0.90f },
    { "room mic",         InstrumentType::Room,            0.90f },
    { "drum room",        InstrumentType::Room,            0.90f },
    { "congregation",     InstrumentType::Room,            0.95f },
    { "audience",         InstrumentType::Room,            0.95f },
    { "crowd",            InstrumentType::Room,            0.95f },
    { "room",             InstrumentType::Room,            0.85f },

    // Kick (Trap: "bass drum" is Kick, NEVER Bass)
    { "bass drum",        InstrumentType::Kick,            0.95f },
    { "kick sub",         InstrumentType::Kick,            0.95f },
    { "kicksub",          InstrumentType::Kick,            0.95f },
    { "kick drum",        InstrumentType::Kick,            0.95f },
    { "kick out",         InstrumentType::Kick,            0.95f },
    { "kickout",          InstrumentType::Kick,            0.95f },
    { "kick in",          InstrumentType::Kick,            0.95f },
    { "kickin",           InstrumentType::Kick,            0.95f },
    { "bd out",           InstrumentType::Kick,            0.95f },
    { "bd in",            InstrumentType::Kick,            0.95f },
    { "kik in",           InstrumentType::Kick,            0.95f },
    { "kik out",          InstrumentType::Kick,            0.95f },
    { "kick",             InstrumentType::Kick,            0.95f },
    { "kik",              InstrumentType::Kick,            0.95f },
    { "bd",               InstrumentType::Kick,            0.90f },

    // Snare
    { "snare bottom",     InstrumentType::Snare,           0.95f },
    { "snare top",        InstrumentType::Snare,           0.95f },
    { "snr bottom",       InstrumentType::Snare,           0.95f },
    { "snr top",          InstrumentType::Snare,           0.95f },
    { "snare bot",        InstrumentType::Snare,           0.95f },
    { "snr bot",          InstrumentType::Snare,           0.95f },
    { "snr t",            InstrumentType::Snare,           0.95f },
    { "snr b",            InstrumentType::Snare,           0.95f },
    { "snare side",       InstrumentType::Snare,           0.90f },
    { "snare",            InstrumentType::Snare,           0.95f },
    { "snr",              InstrumentType::Snare,           0.95f },

    // Hi-Hat
    { "hi hat",           InstrumentType::HiHat,           0.95f },
    { "hihat",            InstrumentType::HiHat,           0.95f },
    { "hats",             InstrumentType::HiHat,           0.95f },
    { "hat",              InstrumentType::HiHat,           0.90f },
    { "hh",               InstrumentType::HiHat,           0.90f },

    // Toms
    { "rack tom 1",       InstrumentType::Toms,            0.90f },
    { "rack tom 2",       InstrumentType::Toms,            0.90f },
    { "rack tom",         InstrumentType::Toms,            0.90f },
    { "floor tom 1",      InstrumentType::Toms,            0.90f },
    { "floor tom 2",      InstrumentType::Toms,            0.90f },
    { "floor tom",        InstrumentType::Toms,            0.90f },
    { "high tom",         InstrumentType::Toms,            0.90f },
    { "mid tom",          InstrumentType::Toms,            0.90f },
    { "low tom",          InstrumentType::Toms,            0.90f },
    { "tom 1",            InstrumentType::Toms,            0.90f },
    { "tom 2",            InstrumentType::Toms,            0.90f },
    { "tom 3",            InstrumentType::Toms,            0.90f },
    { "toms",             InstrumentType::Toms,            0.90f },
    { "tom",              InstrumentType::Toms,            0.85f },
    { "ft",               InstrumentType::Toms,            0.85f },
    { "rt",               InstrumentType::Toms,            0.85f },

    // Vocals - Lead vs BGV vs Pastor
    { "worship leader",   InstrumentType::LeadVocal,       0.95f },
    { "lead vocal",       InstrumentType::LeadVocal,       0.95f },
    { "main vocal",       InstrumentType::LeadVocal,       0.95f },
    { "lead singer",      InstrumentType::LeadVocal,       0.95f },
    { "lead vox",         InstrumentType::LeadVocal,       0.95f },
    { "lead voc",         InstrumentType::LeadVocal,       0.95f },
    { "ld vox",           InstrumentType::LeadVocal,       0.95f },
    { "ldvox",            InstrumentType::LeadVocal,       0.95f },
    { "vox lead",         InstrumentType::LeadVocal,       0.95f },
    { "vocal lead",       InstrumentType::LeadVocal,       0.95f },
    { "vox 1",            InstrumentType::LeadVocal,       0.90f },
    { "vocal 1",          InstrumentType::LeadVocal,       0.90f },

    { "pastor mic",       InstrumentType::PastorSpeaking,  0.95f },
    { "pastor",           InstrumentType::PastorSpeaking,  0.95f },
    { "preaching",        InstrumentType::PastorSpeaking,  0.95f },
    { "speaking",         InstrumentType::PastorSpeaking,  0.95f },
    { "sermon",           InstrumentType::PastorSpeaking,  0.95f },
    { "spoken",           InstrumentType::PastorSpeaking,  0.90f },
    { "announcer",        InstrumentType::PastorSpeaking,  0.90f },
    { "podium",           InstrumentType::PastorSpeaking,  0.90f },
    { "talk",             InstrumentType::PastorSpeaking,  0.85f },
    { "host",             InstrumentType::PastorSpeaking,  0.85f },
    { "mc",               InstrumentType::PastorSpeaking,  0.85f },

    { "backing vocal",    InstrumentType::BackgroundVocal, 0.90f },
    { "backing vox",      InstrumentType::BackgroundVocal, 0.90f },
    { "bg vox",           InstrumentType::BackgroundVocal, 0.90f },
    { "harmony",          InstrumentType::BackgroundVocal, 0.90f },
    { "choir",            InstrumentType::Choir,           0.90f },
    { "vox 2",            InstrumentType::BackgroundVocal, 0.85f },
    { "vox 3",            InstrumentType::BackgroundVocal, 0.85f },
    { "vox 4",            InstrumentType::BackgroundVocal, 0.85f },
    { "vocal 2",          InstrumentType::BackgroundVocal, 0.85f },
    { "vocal 3",          InstrumentType::BackgroundVocal, 0.85f },
    { "vocals",           InstrumentType::BackgroundVocal, 0.80f },
    { "bgv",              InstrumentType::BackgroundVocal, 0.90f },
    { "bvs",              InstrumentType::BackgroundVocal, 0.90f },
    { "bv",               InstrumentType::BackgroundVocal, 0.85f },

    // Generic Vox / Vocal (defaults to LeadVocal if not numbered or marked bgv)
    { "vocal",            InstrumentType::LeadVocal,       0.80f },
    { "vox",              InstrumentType::LeadVocal,       0.80f },
    { "voc",              InstrumentType::LeadVocal,       0.75f },
    { "singer",           InstrumentType::LeadVocal,       0.80f },

    // Bass
    { "bass di",          InstrumentType::Bass,            0.95f },
    { "bass amp",         InstrumentType::Bass,            0.95f },
    { "di bass",          InstrumentType::Bass,            0.95f },
    { "sub bass",         InstrumentType::Bass,            0.95f },
    { "bass guitar",      InstrumentType::Bass,            0.95f },
    { "bass synth",       InstrumentType::Bass,            0.90f },
    { "electric bass",    InstrumentType::Bass,            0.95f },
    { "bass",             InstrumentType::Bass,            0.90f },

    // Guitars - Traps: "lead gtr" is ElectricGuitar, NEVER LeadVocal!
    { "acoustic guitar",  InstrumentType::AcousticGuitar,  0.95f },
    { "acoustic gtr",     InstrumentType::AcousticGuitar,  0.95f },
    { "ac guitar",        InstrumentType::AcousticGuitar,  0.95f },
    { "ac gtr",           InstrumentType::AcousticGuitar,  0.95f },
    { "agtr",             InstrumentType::AcousticGuitar,  0.95f },
    { "steel string",     InstrumentType::AcousticGuitar,  0.95f },
    { "acoustic",         InstrumentType::AcousticGuitar,  0.90f },
    { "ag",               InstrumentType::AcousticGuitar,  0.85f },

    { "electric guitar",  InstrumentType::ElectricGuitar,  0.95f },
    { "electric gtr",     InstrumentType::ElectricGuitar,  0.95f },
    { "elec guitar",      InstrumentType::ElectricGuitar,  0.95f },
    { "elec gtr",         InstrumentType::ElectricGuitar,  0.95f },
    { "lead guitar",      InstrumentType::ElectricGuitar,  0.95f },
    { "lead gtr",         InstrumentType::ElectricGuitar,  0.95f },
    { "eg lead",          InstrumentType::ElectricGuitar,  0.95f },
    { "lead eg",          InstrumentType::ElectricGuitar,  0.95f },
    { "rhythm guitar",    InstrumentType::ElectricGuitar,  0.95f },
    { "rhythm gtr",       InstrumentType::ElectricGuitar,  0.95f },
    { "eg rhythm",        InstrumentType::ElectricGuitar,  0.95f },
    { "rhythm eg",        InstrumentType::ElectricGuitar,  0.95f },
    { "egtr",             InstrumentType::ElectricGuitar,  0.95f },
    { "eg l",             InstrumentType::ElectricGuitar,  0.95f },
    { "eg r",             InstrumentType::ElectricGuitar,  0.95f },
    { "electric",         InstrumentType::ElectricGuitar,  0.90f },
    { "eg",               InstrumentType::ElectricGuitar,  0.85f },
    { "guitar",           InstrumentType::ElectricGuitar,  0.85f },
    { "gtr",              InstrumentType::ElectricGuitar,  0.85f },

    // Piano vs Keys
    { "grand piano",      InstrumentType::Piano,           0.95f },
    { "upright piano",    InstrumentType::Piano,           0.95f },
    { "acoustic piano",   InstrumentType::Piano,           0.95f },
    { "keys piano",       InstrumentType::Piano,           0.95f },
    { "piano",            InstrumentType::Piano,           0.95f },
    { "grand",            InstrumentType::Piano,           0.90f },
    { "pno",              InstrumentType::Piano,           0.90f },

    { "synth keys",       InstrumentType::Keys,            0.90f },
    { "keyboard",         InstrumentType::Keys,            0.90f },
    { "wurlitzer",        InstrumentType::Keys,            0.95f },
    { "rhodes",           InstrumentType::Keys,            0.95f },
    { "wurli",            InstrumentType::Keys,            0.95f },
    { "organ",            InstrumentType::Keys,            0.95f },
    { "clav",             InstrumentType::Keys,            0.90f },
    { "nord",             InstrumentType::Keys,            0.90f },
    { "b3",               InstrumentType::Keys,            0.95f },
    { "keys",             InstrumentType::Keys,            0.90f },

    // Pad vs Synth
    { "strings pad",      InstrumentType::Pad,             0.90f },
    { "synth pad",        InstrumentType::Pad,             0.90f },
    { "ambient pad",      InstrumentType::Pad,             0.90f },
    { "atmosphere",       InstrumentType::Pad,             0.90f },
    { "shimmer",          InstrumentType::Pad,             0.90f },
    { "ambient",          InstrumentType::Pad,             0.85f },
    { "pad",              InstrumentType::Pad,             0.90f },

    { "synth lead",       InstrumentType::Synth,           0.90f },
    { "synth bass",       InstrumentType::Synth,           0.90f },
    { "lead synth",       InstrumentType::Synth,           0.90f },
    { "sequence",         InstrumentType::Synth,           0.90f },
    { "arps",             InstrumentType::Synth,           0.90f },
    { "arp",              InstrumentType::Synth,           0.90f },
    { "seq",              InstrumentType::Synth,           0.85f },
    { "synth",            InstrumentType::Synth,           0.85f },

    // Tracks / Playback / Stems
    { "click+tracks",     InstrumentType::Tracks,          0.90f },
    { "backing track",    InstrumentType::Tracks,          0.90f },
    { "multitrack",       InstrumentType::Tracks,          0.90f },
    { "playback",         InstrumentType::Tracks,          0.90f },
    { "tracks",           InstrumentType::Tracks,          0.90f },
    { "stems",            InstrumentType::Tracks,          0.90f },
    { "stem",             InstrumentType::Tracks,          0.90f },
    { "loop",             InstrumentType::Tracks,          0.85f },
    { "guide",            InstrumentType::Tracks,          0.85f },
    { "cues",             InstrumentType::Tracks,          0.85f },

    // Click (Special pass-through type)
    { "guide click",      InstrumentType::Click,           1.00f },
    { "click track",      InstrumentType::Click,           1.00f },
    { "metronome",        InstrumentType::Click,           1.00f },
    { "click",            InstrumentType::Click,           1.00f }
};

} // namespace

std::string TrackNameParser::normalizeName(std::string_view rawName) {
    if (rawName.empty()) return "";

    std::string result;
    result.reserve(rawName.size() + 4);

    char prevRawChar = ' ';
    for (char c : rawName) {
        // Split camelCase / PascalCase transitions (e.g. LdVox -> ld vox, KickIn -> kick in)
        if (std::islower(static_cast<unsigned char>(prevRawChar)) && std::isupper(static_cast<unsigned char>(c))) {
            result.push_back(' ');
        }

        char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        // Split transitions between letter and digit (e.g. tom1 -> tom 1, 01ld -> 01 ld)
        if (std::isalpha(static_cast<unsigned char>(prevRawChar)) && std::isdigit(static_cast<unsigned char>(lower))) {
            result.push_back(' ');
        } else if (std::isdigit(static_cast<unsigned char>(prevRawChar)) && std::isalpha(static_cast<unsigned char>(lower))) {
            result.push_back(' ');
        }

        // Punctuation or space to single space
        if (std::isalnum(static_cast<unsigned char>(lower))) {
            result.push_back(lower);
        } else {
            if (!result.empty() && result.back() != ' ') {
                result.push_back(' ');
            }
        }

        prevRawChar = c;
    }

    // Trim trailing whitespace
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    // Trim leading whitespace
    size_t start = 0;
    while (start < result.size() && result[start] == ' ') {
        start++;
    }
    if (start > 0) {
        result = result.substr(start);
    }

    return result;
}

SideHint TrackNameParser::parseSideHint(std::string_view rawName) {
    if (rawName.empty()) return SideHint::Centre;

    std::string s;
    s.reserve(rawName.size());
    for (char c : rawName) {
        s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }

    auto endsWith = [&](const std::string& suffix) {
        if (s.size() < suffix.size()) return false;
        return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    };

    if (endsWith("(l)") || endsWith("(left)") || endsWith("[l]") || endsWith("_l") || endsWith("_left") ||
        endsWith(" - l") || endsWith(" - left") || endsWith(" l") || endsWith(" left")) {
        return SideHint::Left;
    }

    if (endsWith("(r)") || endsWith("(right)") || endsWith("[r]") || endsWith("_r") || endsWith("_right") ||
        endsWith(" - r") || endsWith(" - right") || endsWith(" r") || endsWith(" right")) {
        return SideHint::Right;
    }

    return SideHint::Centre;
}

ParseResult TrackNameParser::parse(std::string_view trackName) {
    SideHint side = parseSideHint(trackName);
    std::string norm = normalizeName(trackName);
    if (norm.empty()) {
        return { InstrumentType::Unknown, 0.0f, "", side };
    }

    // Pad with spaces for strict word-boundary matching (" " + norm + " ")
    std::string padded = " " + norm + " ";

    // Context trap checks:
    // 1. "bass drum": if text has "drum" with "bass", it MUST match Kick, not Bass.
    // Checked automatically since "bass drum" is evaluated before "bass".
    // 2. "lead gtr": if text has "gtr" or "guitar", plain "lead" should NOT match LeadVocal.
    bool hasGuitarContext = (padded.find(" gtr ") != std::string::npos ||
                             padded.find(" guitar ") != std::string::npos ||
                             padded.find(" electric ") != std::string::npos ||
                             padded.find(" eg ") != std::string::npos);

    const AliasEntry* bestMatch = nullptr;
    size_t longestMatchLen = 0;

    for (const auto& entry : ALIAS_DICTIONARY) {
        std::string pattern = " " + std::string(entry.alias) + " ";
        if (padded.find(pattern) != std::string::npos) {
            if (entry.alias.length() > longestMatchLen) {
                longestMatchLen = entry.alias.length();
                bestMatch = &entry;
            }
        }
    }

    if (bestMatch != nullptr) {
        return { bestMatch->type, bestMatch->confidence, std::string(bestMatch->alias), side };
    }

    // Check for plain "lead" alone
    if (padded.find(" lead ") != std::string::npos) {
        if (hasGuitarContext) {
            return { InstrumentType::ElectricGuitar, 0.85f, "lead gtr", side };
        }
        return { InstrumentType::LeadVocal, 0.85f, "lead", side };
    }

    return { InstrumentType::Unknown, 0.0f, "", side };
}

BusType TrackNameParser::parseBusType(std::string_view trackName) {
    std::string norm = normalizeName(trackName);
    std::string padded = " " + norm + " ";

    struct BusAlias {
        std::string_view alias;
        BusType type;
    };

    static const BusAlias BUS_ALIASES[] = {
        { "drum bus",       BusType::Drums },
        { "drums bus",      BusType::Drums },
        { "drum group",     BusType::Drums },
        { "drums group",    BusType::Drums },
        { "drum sub",       BusType::Drums },
        { "drums sub",      BusType::Drums },
        { "drum grp",       BusType::Drums },
        { "drums grp",      BusType::Drums },
        { "kit bus",        BusType::Drums },
        { "drums",          BusType::Drums },
        { "drum",           BusType::Drums },

        { "vocal bus",      BusType::Vocals },
        { "vocal sub",      BusType::Vocals },
        { "vocals sub",     BusType::Vocals },
        { "vox sub",        BusType::Vocals },
        { "vocal group",    BusType::Vocals },
        { "vocals group",   BusType::Vocals },
        { "vox group",      BusType::Vocals },
        { "vox bus",        BusType::Vocals },
        { "bgv bus",        BusType::Vocals },
        { "lead vocal bus", BusType::Vocals },
        { "vocals",         BusType::Vocals },
        { "vocal",          BusType::Vocals },
        { "vox",            BusType::Vocals },

        { "music bus",      BusType::Music },
        { "music sub",      BusType::Music },
        { "music group",    BusType::Music },
        { "band bus",       BusType::Music },
        { "band group",     BusType::Music },
        { "inst bus",       BusType::Music },
        { "instruments",    BusType::Music },
        { "band",           BusType::Music },
        { "music",          BusType::Music },

        { "reverb return",  BusType::FX },
        { "delay return",   BusType::FX },
        { "verb return",    BusType::FX },
        { "fx return",      BusType::FX },
        { "reverb bus",     BusType::FX },
        { "delay bus",      BusType::FX },
        { "verb bus",       BusType::FX },
        { "fx bus",         BusType::FX },
        { "effects bus",    BusType::FX },
        { "effects",        BusType::FX },
        { "reverb",         BusType::FX },
        { "verb",           BusType::FX },
        { "fx",             BusType::FX }
    };

    const BusAlias* bestMatch = nullptr;
    size_t longestMatchLen = 0;

    for (const auto& entry : BUS_ALIASES) {
        std::string pattern = " " + std::string(entry.alias) + " ";
        if (padded.find(pattern) != std::string::npos) {
            if (entry.alias.length() > longestMatchLen) {
                longestMatchLen = entry.alias.length();
                bestMatch = &entry;
            }
        }
    }

    if (bestMatch != nullptr) {
        return bestMatch->type;
    }

    return BusType::Generic;
}

} // namespace aimixer
