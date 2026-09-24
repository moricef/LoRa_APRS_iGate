#ifndef APRS_TELEMETRY_RX_H_
#define APRS_TELEMETRY_RX_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace APRS_Telemetry_RX {

constexpr size_t kAnalogChannels = 5;
constexpr size_t kDigitalChannels = 8;
constexpr size_t kMetadataFields = kAnalogChannels + kDigitalChannels;
constexpr size_t kMaxStations = 10;
constexpr size_t kLabelBytes = 8; // APRS labels are at most seven characters plus NUL.

struct Equation {
    float a = 0.0F;
    float b = 1.0F;
    float c = 0.0F;
    bool defined = false;
};

struct Station {
    std::string callsign;
    std::string rxTime;
    std::string sequence;
    std::string format;
    std::array<char, 24> project{}; // APRS BITS project title: 0..23 characters.
    uint32_t receivedAtMillis = 0;
    uint32_t touchedAtMillis = 0;
    bool hasData = false;
    std::array<float, kAnalogChannels> analog{};
    size_t analogCount = 0;
    bool hasDigital = false;
    uint8_t digital = 0;
    std::array<std::array<char, kLabelBytes>, kMetadataFields> names{};
    std::array<std::array<char, kLabelBytes>, kMetadataFields> units{};
    std::array<Equation, kAnalogChannels> equations{};
    std::array<bool, kDigitalChannels> bitSense{};
    bool hasBitSense = false;
};

// Retains only the latest values and metadata for a bounded set of stations.
// No sample history or raw packet copy is kept on the ESP32.
class Store {
public:
    bool ingest(const std::string& packet, const std::string& rxTime, uint32_t nowMillis);
    const std::vector<Station>& stations() const { return stations_; }
    void clear() { stations_.clear(); }

private:
    Station& stationFor(const std::string& callsign, uint32_t nowMillis);
    std::vector<Station> stations_;
};

float calibratedValue(const Station& station, size_t channel);
bool metadataDescriptor(const std::string& packet, std::string& station, std::string& kind);

} // namespace APRS_Telemetry_RX

#endif
