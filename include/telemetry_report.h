#pragma once

#include <cstdio>
#include <string>

namespace TelemetryReport {

inline bool needsSeparateReport(bool weatherSensor, char symbol) {
    return weatherSensor || symbol == '_';
}

// Reuse the one sampled base91 report so voltage scaling, activity counters
// and channel ordering stay identical to PARM/UNIT/EQNS. APRS extended T#
// reports (supported by aprs.fi) allow values above 255 and omitted channels.
inline std::string fromBase91(const std::string& encoded) {
    if (encoded.size() < 6 || encoded.size() > 14 ||
        encoded.front() != '|' || encoded.back() != '|' || encoded.size() % 2 != 0)
        return {};
    std::string result;
    for (size_t i = 1; i + 1 < encoded.size() - 1; i += 2) {
        const unsigned char a = encoded[i], b = encoded[i + 1];
        if (a < 33 || a > 123 || b < 33 || b > 123) return {};
        const unsigned value = (a - 33) * 91 + b - 33;
        if (i == 1) {
            char sequence[8];
            std::snprintf(sequence, sizeof(sequence), "T#%03u", value % 1000);
            result = sequence;
        } else {
            result += ',';
            result += std::to_string(value);
        }
    }
    return result;
}

} // namespace TelemetryReport
