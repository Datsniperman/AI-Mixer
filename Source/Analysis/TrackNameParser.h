#pragma once

#include "../Common/Types.h"
#include <string>
#include <string_view>

namespace aimixer {

struct ParseResult {
    InstrumentType type = InstrumentType::Unknown;
    float confidence = 0.0f;
    std::string matchedAlias;
    SideHint side = SideHint::Centre;
};

class TrackNameParser {
public:
    static ParseResult parse(std::string_view trackName);

    // Extracts side hint { Left, Right, Centre } from name suffixes before stripping
    static SideHint parseSideHint(std::string_view rawName);

    // Parses BusType from track name (scoped to Bus role)
    static BusType parseBusType(std::string_view trackName);

    // Helper to normalize track names (lowercase, space-separated, no punctuation)
    static std::string normalizeName(std::string_view rawName);
};

} // namespace aimixer
