#include "aprs_telemetry_persistence.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>

namespace {

constexpr const char* kMetadataPath = "/aprs_tlm_meta.json";

bool readMetadata(JsonDocument& document) {
    File file = SPIFFS.open(kMetadataPath, "r");
    if (!file) {
        document.to<JsonArray>();
        return false;
    }
    const DeserializationError error = deserializeJson(document, file);
    file.close();
    if (error || !document.is<JsonArray>()) {
        document.clear();
        document.to<JsonArray>();
        return false;
    }
    return true;
}

} // namespace

namespace APRS_Telemetry_Persistence {

void load(APRS_Telemetry_RX::Store& store) {
    JsonDocument document;
    if (!readMetadata(document)) return;

    for (JsonObject station : document.as<JsonArray>()) {
        JsonObject packets = station["packets"];
        for (JsonPair entry : packets) {
            const char* packet = entry.value().as<const char*>();
            if (packet != nullptr) store.ingest(packet, "", 0);
        }
    }
}

bool remember(const String& packet) {
    std::string stationName;
    std::string kind;
    if (!APRS_Telemetry_RX::metadataDescriptor(packet.c_str(), stationName, kind)) return false;

    JsonDocument document;
    readMetadata(document);
    JsonArray stations = document.as<JsonArray>();
    JsonObject station;
    for (JsonObject candidate : stations) {
        if (candidate["station"].as<std::string>() == stationName) {
            station = candidate;
            break;
        }
    }

    if (station.isNull()) {
        if (stations.size() >= APRS_Telemetry_RX::kMaxStations) stations.remove(0);
        station = stations.add<JsonObject>();
        station["station"] = stationName;
    }

    const char* existing = station["packets"][kind].as<const char*>();
    if (existing != nullptr && packet == existing) return true;
    station["packets"][kind] = packet;

    File file = SPIFFS.open(kMetadataPath, "w");
    if (!file) return true;
    serializeJson(document, file);
    file.close();
    return true;
}

} // namespace APRS_Telemetry_Persistence
