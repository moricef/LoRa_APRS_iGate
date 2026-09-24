#include "aprs_telemetry_rx.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace {

using APRS_Telemetry_RX::Station;

std::string trimRight(std::string value) {
    while (!value.empty() && value.back() == ' ') value.pop_back();
    return value;
}

std::vector<std::string> split(const std::string& value) {
    std::vector<std::string> fields;
    size_t start = 0;
    while (start <= value.size()) {
        const size_t end = value.find(',', start);
        fields.push_back(value.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return fields;
}

template <size_t Size>
void copyBounded(std::array<char, Size>& destination, const std::string& value) {
    const size_t count = std::min(value.size(), Size - 1);
    std::copy(value.begin(), value.begin() + count, destination.begin());
    destination[count] = '\0';
    std::fill(destination.begin() + count + 1, destination.end(), '\0');
}

bool parseFloat(const std::string& text, float& value) {
    if (text.empty()) return false;
    char* end = nullptr;
    errno = 0;
    value = std::strtof(text.c_str(), &end);
    return errno == 0 && end == text.c_str() + text.size() && std::isfinite(value);
}

bool parseTraditional(const std::string& info, Station& station) {
    if (info.compare(0, 2, "T#") != 0 || info.size() < 5) return false;

    size_t valuesStart = 5;
    station.sequence = info.substr(2, 3);
    if (station.sequence == "MIC") {
        valuesStart = info.size() > 5 && info[5] == ',' ? 6 : 5;
    } else {
        for (char c : station.sequence) {
            if (c < '0' || c > '9') return false;
        }
        if (info.size() <= 5 || info[5] != ',') return false;
        valuesStart = 6;
    }

    const std::vector<std::string> fields = split(info.substr(valuesStart));
    if (fields.size() < 6) return false;

    std::array<float, APRS_Telemetry_RX::kAnalogChannels> values{};
    for (size_t i = 0; i < values.size(); ++i) {
        if (!parseFloat(fields[i], values[i])) return false;
    }

    const std::string& bits = fields[5];
    if (bits.size() < APRS_Telemetry_RX::kDigitalChannels) return false;
    uint8_t digital = 0;
    for (size_t i = 0; i < APRS_Telemetry_RX::kDigitalChannels; ++i) {
        if (bits[i] != '0' && bits[i] != '1') return false;
        if (bits[i] == '1') digital |= static_cast<uint8_t>(1U << i);
    }

    station.analog = values;
    station.analogCount = values.size();
    station.hasDigital = true;
    station.digital = digital;
    station.format = "T#";
    return true;
}

bool decodeBase91Pair(const std::string& text, size_t offset, uint16_t& value) {
    if (offset + 1 >= text.size()) return false;
    const unsigned char high = static_cast<unsigned char>(text[offset]);
    const unsigned char low = static_cast<unsigned char>(text[offset + 1]);
    if (high < 33 || high > 123 || low < 33 || low > 123) return false;
    value = static_cast<uint16_t>((high - 33) * 91 + (low - 33));
    return true;
}

bool parseBase91(const std::string& info, Station& station) {
    size_t start = info.find('|');
    while (start != std::string::npos) {
        const size_t end = info.find('|', start + 1);
        if (end == std::string::npos) return false;
        const std::string encoded = info.substr(start + 1, end - start - 1);
        const size_t pairs = encoded.size() / 2;
        if (encoded.size() >= 4 && encoded.size() <= 14 && encoded.size() % 2 == 0) {
            std::array<uint16_t, 7> decoded{};
            bool valid = true;
            for (size_t i = 0; i < pairs; ++i) {
                if (!decodeBase91Pair(encoded, i * 2, decoded[i])) {
                    valid = false;
                    break;
                }
            }
            if (valid) {
                station.sequence = std::to_string(decoded[0]);
                station.analogCount = std::min(pairs - 1, APRS_Telemetry_RX::kAnalogChannels);
                for (size_t i = 0; i < station.analogCount; ++i) station.analog[i] = decoded[i + 1];
                station.hasDigital = pairs == 7;
                station.digital = station.hasDigital ? static_cast<uint8_t>(decoded[6] & 0xffU) : 0;
                station.format = "Base91";
                return true;
            }
        }
        start = info.find('|', end + 1);
    }
    return false;
}

bool metadataMessage(const std::string& info, std::string& addressee, std::string& body) {
    if (info.size() < 12 || info[0] != ':' || info[10] != ':') return false;
    body = info.substr(11);
    if (body.compare(0, 5, "PARM.") != 0 && body.compare(0, 5, "UNIT.") != 0 &&
        body.compare(0, 5, "EQNS.") != 0 && body.compare(0, 5, "BITS.") != 0) return false;
    addressee = trimRight(info.substr(1, 9));
    return !addressee.empty();
}

void applyMetadata(Station& station, const std::string& body) {
    const std::string kind = body.substr(0, 5);
    const std::vector<std::string> fields = split(body.substr(5));
    if (kind == "PARM." || kind == "UNIT.") {
        auto& target = kind == "PARM." ? station.names : station.units;
        const size_t count = std::min(fields.size(), target.size());
        for (size_t i = 0; i < count; ++i) copyBounded(target[i], fields[i]);
        return;
    }
    if (kind == "EQNS.") {
        const size_t count = std::min(fields.size() / 3, station.equations.size());
        for (size_t i = 0; i < count; ++i) {
            float a = 0.0F, b = 0.0F, c = 0.0F;
            if (parseFloat(fields[i * 3], a) && parseFloat(fields[i * 3 + 1], b) &&
                parseFloat(fields[i * 3 + 2], c)) {
                station.equations[i].a = a;
                station.equations[i].b = b;
                station.equations[i].c = c;
                station.equations[i].defined = true;
            }
        }
        return;
    }

    if (fields.empty() || fields[0].size() < APRS_Telemetry_RX::kDigitalChannels) return;
    for (size_t i = 0; i < APRS_Telemetry_RX::kDigitalChannels; ++i) {
        if (fields[0][i] != '0' && fields[0][i] != '1') return;
        station.bitSense[i] = fields[0][i] == '1';
    }
    station.hasBitSense = true;
    std::string project;
    for (size_t i = 1; i < fields.size(); ++i) {
        if (!project.empty()) project += ',';
        project += fields[i];
    }
    copyBounded(station.project, project);
}

bool unwrapTnc2(const std::string& packet, std::string& source, std::string& info) {
    std::string current = packet;
    for (unsigned int depth = 0; depth < 4; ++depth) {
        const size_t gt = current.find('>');
        const size_t colon = current.find(':', gt == std::string::npos ? 0 : gt + 1);
        if (gt == std::string::npos || gt == 0 || colon == std::string::npos) return false;
        source = current.substr(0, gt);
        info = current.substr(colon + 1);
        if (info.empty() || info[0] != '}') return true;
        current = info.substr(1);
    }
    return false;
}

} // namespace

namespace APRS_Telemetry_RX {

Station& Store::stationFor(const std::string& callsign, uint32_t nowMillis) {
    for (Station& station : stations_) {
        if (station.callsign == callsign) return station;
    }
    if (stations_.size() >= kMaxStations) {
        auto oldest = std::min_element(stations_.begin(), stations_.end(), [](const Station& left, const Station& right) {
            return left.touchedAtMillis < right.touchedAtMillis;
        });
        stations_.erase(oldest);
    }
    stations_.push_back(Station{});
    stations_.back().callsign = callsign;
    stations_.back().touchedAtMillis = nowMillis;
    return stations_.back();
}

bool Store::ingest(const std::string& packet, const std::string& rxTime, uint32_t nowMillis) {
    std::string source;
    std::string info;
    if (!unwrapTnc2(packet, source, info)) return false;

    std::string addressee;
    std::string body;
    if (metadataMessage(info, addressee, body)) {
        Station& station = stationFor(addressee, nowMillis);
        applyMetadata(station, body);
        station.touchedAtMillis = nowMillis;
        return true;
    }

    Station parsed;
    if (!parseTraditional(info, parsed) && !parseBase91(info, parsed)) return false;

    Station& station = stationFor(source, nowMillis);
    station.sequence = parsed.sequence;
    station.format = parsed.format;
    station.analog = parsed.analog;
    station.analogCount = parsed.analogCount;
    station.hasDigital = parsed.hasDigital;
    station.digital = parsed.digital;
    station.rxTime = rxTime;
    station.receivedAtMillis = nowMillis;
    station.touchedAtMillis = nowMillis;
    station.hasData = true;
    return true;
}

float calibratedValue(const Station& station, size_t channel) {
    if (channel >= station.analogCount) return std::numeric_limits<float>::quiet_NaN();
    const float raw = station.analog[channel];
    const Equation& equation = station.equations[channel];
    if (!equation.defined) return raw;
    return equation.a * raw * raw + equation.b * raw + equation.c;
}

bool metadataDescriptor(const std::string& packet, std::string& station, std::string& kind) {
    std::string source;
    std::string info;
    std::string body;
    if (!unwrapTnc2(packet, source, info) || !metadataMessage(info, station, body)) return false;
    kind = body.substr(0, 4);
    return true;
}

} // namespace APRS_Telemetry_RX
