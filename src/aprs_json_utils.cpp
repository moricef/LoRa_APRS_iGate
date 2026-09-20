/* Copyright (C) 2026 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate and is distributed under GPLv3.
 */

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <esp_system.h>
#include <mbedtls/base64.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "aprs_json_utils.h"
#include "configuration.h"
#include "lora_utils.h"

extern Configuration Config;
extern String versionNumber;

namespace {

constexpr size_t EVENT_QUEUE_SIZE = 10;
constexpr size_t MAX_RECORD_BYTES = 4096;

struct StoredEvent {
    uint32_t sequence;
    String jsonLine;
};

struct StreamState {
    bool helloPending = true;
    uint32_t nextSequence = 1;
    size_t pendingOffset = 0;
    String pending;
};

std::vector<StoredEvent> eventQueue;
SemaphoreHandle_t eventMutex = nullptr;
String bootId;
uint32_t latestSequence = 0;

String base64Encode(const String& value) {
    size_t encodedLength = 0;
    mbedtls_base64_encode(nullptr, 0, &encodedLength,
                          reinterpret_cast<const unsigned char*>(value.c_str()), value.length());
    if (encodedLength == 0) return "";

    std::unique_ptr<unsigned char[]> encoded(new unsigned char[encodedLength + 1]);
    size_t written = 0;
    if (mbedtls_base64_encode(encoded.get(), encodedLength, &written,
                              reinterpret_cast<const unsigned char*>(value.c_str()), value.length()) != 0) {
        return "";
    }
    encoded[written] = '\0';
    return String(reinterpret_cast<const char*>(encoded.get()), written);
}

bool isValidUtf8(const String& value) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(value.c_str());
    size_t i = 0;
    while (i < value.length()) {
        uint8_t c = bytes[i++];
        if (c <= 0x7f) continue;

        size_t remaining = 0;
        uint32_t codepoint = 0;
        if ((c & 0xe0) == 0xc0) {
            remaining = 1;
            codepoint = c & 0x1f;
            if (codepoint < 2) return false;
        } else if ((c & 0xf0) == 0xe0) {
            remaining = 2;
            codepoint = c & 0x0f;
        } else if ((c & 0xf8) == 0xf0) {
            remaining = 3;
            codepoint = c & 0x07;
        } else {
            return false;
        }
        if (i + remaining > value.length()) return false;
        while (remaining--) {
            uint8_t continuation = bytes[i++];
            if ((continuation & 0xc0) != 0x80) return false;
            codepoint = (codepoint << 6) | (continuation & 0x3f);
        }
        if ((codepoint >= 0xd800 && codepoint <= 0xdfff) || codepoint > 0x10ffff) return false;
        if ((codepoint <= 0x7ff && (c & 0xf0) == 0xe0) ||
            (codepoint <= 0xffff && (c & 0xf8) == 0xf0)) return false;
    }
    return true;
}

String hardwareName() {
#if defined(TTGO_LORA32_V2_1)
    return "LilyGo TTGO LoRa32 V2.1";
#elif defined(HELTEC_V3_2)
    return "Heltec WiFi LoRa 32 V3.2";
#elif defined(LIGHTGATEWAY_PLUS_1_0)
    return "QRP Labs LightGateway Plus 1.0";
#elif defined(ESP32S3)
    return "ESP32-S3";
#elif defined(ESP32C3)
    return "ESP32-C3";
#else
    return "ESP32";
#endif
}

void addAddress(JsonObject object, const String& rawText, bool pathElement = false) {
    String text = rawText;
    bool repeated = pathElement && text.endsWith("*");
    if (repeated) text.remove(text.length() - 1);

    object["text"] = rawText;
    if (pathElement) object["repeated"] = repeated;

    int dash = text.indexOf('-');
    String call = dash > 0 ? text.substring(0, dash) : text;
    if (call.length() > 0) object["call"] = call;
    if (dash > 0 && dash + 1 < static_cast<int>(text.length())) {
        String suffix = text.substring(dash + 1);
        object["suffix"] = suffix;
        bool numeric = true;
        uint32_t ssid = 0;
        for (size_t i = 0; i < suffix.length(); ++i) {
            if (!isDigit(static_cast<unsigned char>(suffix[i]))) {
                numeric = false;
                break;
            }
            ssid = ssid * 10 + static_cast<uint8_t>(suffix[i] - '0');
        }
        if (numeric) object["ssid"] = ssid;
    }

    if (pathElement) {
        String upper = text;
        upper.toUpperCase();
        if (upper.startsWith("QA") && upper.length() >= 3) {
            object["kind"] = "q_construct";
        } else if (upper == "TCPIP" || upper == "TCPXX" || upper == "NOGATE" || upper == "RFONLY") {
            object["kind"] = "internet";
        } else if (upper.startsWith("WIDE") || upper.startsWith("TRACE")) {
            object["kind"] = "alias";
        } else if (call.length() > 0) {
            object["kind"] = "station";
        } else {
            object["kind"] = "unknown";
        }
    }
}

bool addParsedPacket(JsonObject packetObject, const String& cleanPacket) {
    int gt = cleanPacket.indexOf('>');
    int colon = cleanPacket.indexOf(':', gt + 1);
    if (gt <= 0 || colon <= gt + 1) return false;

    String source = cleanPacket.substring(0, gt);
    String route = cleanPacket.substring(gt + 1, colon);
    int firstComma = route.indexOf(',');
    String destination = firstComma < 0 ? route : route.substring(0, firstComma);
    if (destination.length() == 0) return false;

    addAddress(packetObject["source"].to<JsonObject>(), source);
    addAddress(packetObject["destination"].to<JsonObject>(), destination);

    JsonArray path = packetObject["path"].to<JsonArray>();
    if (firstComma >= 0) {
        int start = firstComma + 1;
        while (start <= static_cast<int>(route.length())) {
            int comma = route.indexOf(',', start);
            String element = comma < 0 ? route.substring(start) : route.substring(start, comma);
            if (element.length() == 0) return false;
            addAddress(path.add<JsonObject>(), element, true);
            if (comma < 0) break;
            start = comma + 1;
        }
    }

    String information = cleanPacket.substring(colon + 1);
    JsonObject info = packetObject["information"].to<JsonObject>();
    info["raw_base64"] = base64Encode(information);
    if (isValidUtf8(information)) info["text"] = information;
    if (information.length() > 0) {
        uint8_t dti = static_cast<uint8_t>(information[0]);
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", dti);
        info["dti_hex"] = hex;
        if (dti == 0x1c || dti == 0x1d || (dti >= 0x20 && dti <= 0x7e)) {
            char dtiText[2] = {static_cast<char>(dti), '\0'};
            info["dti"] = dtiText;
        }
    }
    return true;
}

String buildHello(uint32_t sequenceAtOpen) {
    JsonDocument document;
    document["protocol"] = "lora-aprs-json";
    document["protocol_version"] = "1";
    document["schema_version"] = "1.0";
    document["event"] = "hello";
    document["boot_id"] = bootId;
    document["uptime_ms"] = millis();
    document["latest_sequence"] = sequenceAtOpen;
    document["producer"]["station"] = Config.callsign;
    document["producer"]["software"] = "LoRa_APRS_iGate";
    document["producer"]["firmware"] = versionNumber;
    document["producer"]["hardware"] = hardwareName();

    JsonArray events = document["capabilities"]["events"].to<JsonArray>();
    events.add("rx");
    JsonArray features = document["capabilities"]["features"].to<JsonArray>();
    features.add("local_metrics");
    features.add("radio_parameters");
    features.add("rxt");
    document["capabilities"]["transports"].to<JsonArray>().add("ndjson-http");
    document["capabilities"]["max_record_bytes"] = MAX_RECORD_BYTES;

    String output;
    serializeJson(document, output);
    output += '\n';
    return output;
}

bool copyQueuedEvent(uint32_t sequence, String& output, uint32_t& oldestSequence) {
    if (eventMutex == nullptr) return false;
    xSemaphoreTake(eventMutex, portMAX_DELAY);
    oldestSequence = eventQueue.empty() ? latestSequence + 1 : eventQueue.front().sequence;
    for (const StoredEvent& stored : eventQueue) {
        if (stored.sequence == sequence) {
            output = stored.jsonLine;
            xSemaphoreGive(eventMutex);
            return true;
        }
    }
    xSemaphoreGive(eventMutex);
    return false;
}

uint32_t sequenceSnapshot() {
    if (eventMutex == nullptr) return 0;
    xSemaphoreTake(eventMutex, portMAX_DELAY);
    uint32_t value = latestSequence;
    xSemaphoreGive(eventMutex);
    return value;
}

} // namespace

namespace APRS_JSON_Utils {

void setup() {
    if (eventMutex == nullptr) eventMutex = xSemaphoreCreateMutex();
    char id[9];
    snprintf(id, sizeof(id), "%08lx", static_cast<unsigned long>(esp_random()));
    bootId = id;
}

void recordRx(const String& rfPacket,
              const LoRa_Utils::RxtRxContext& localRx,
              const String& rawRxtField,
              const std::vector<LoRa_Utils::RxtHopMetric>& hopMetrics) {
    if (eventMutex == nullptr) return;

    String cleanPacket = LoRa_Utils::stripRxtTrailer(rfPacket);
    uint32_t sequence;
    xSemaphoreTake(eventMutex, portMAX_DELAY);
    sequence = latestSequence + 1;
    xSemaphoreGive(eventMutex);

    JsonDocument document;
    document["protocol"] = "lora-aprs-json";
    document["protocol_version"] = "1";
    document["schema_version"] = "1.0";
    document["event"] = "rx";
    document["event_id"] = bootId + ":" + String(sequence);
    document["boot_id"] = bootId;
    document["sequence"] = sequence;
    document["uptime_ms"] = millis();
    document["receiver"]["station"] = Config.callsign;
    document["receiver"]["interface"] = "lora0";

    JsonObject packetObject = document["packet"].to<JsonObject>();
    packetObject["raw_tnc2_base64"] = base64Encode(cleanPacket);
    if (rawRxtField.length() > 0) packetObject["rf_tnc2_base64"] = base64Encode(rfPacket);
    if (isValidUtf8(cleanPacket)) packetObject["tnc2"] = cleanPacket;
    bool parsed = addParsedPacket(packetObject, cleanPacket);
    packetObject["parse_status"] = parsed ? "parsed" : "malformed";

    JsonObject reception = document["reception"].to<JsonObject>();
    reception["crc_valid"] = true;
    if (localRx.valid) {
        reception["local"]["rssi_dbm"] = localRx.rssi;
        reception["local"]["snr_db"] = localRx.snr;
        reception["local"]["frequency_error_hz"] = localRx.fo;
    }
    reception["radio"]["frequency_hz"] = Config.loramodule.rxFreq;
    reception["radio"]["bandwidth_hz"] = Config.loramodule.rxSignalBandwidth;
    reception["radio"]["spreading_factor"] = Config.loramodule.rxSpreadingFactor;
    reception["radio"]["coding_rate"] = "4/" + String(Config.loramodule.rxCodingRate4);

    if (rawRxtField.length() > 0) {
        JsonObject rxt = reception["rxt"].to<JsonObject>();
        rxt["encoding"] = "rxt-v1";
        rxt["raw"] = rawRxtField;
        JsonArray hops = rxt["hops"].to<JsonArray>();
        uint32_t ordinal = 1;
        for (auto it = hopMetrics.rbegin(); it != hopMetrics.rend(); ++it) {
            if (!it->hasData) continue;
            JsonObject hop = hops.add<JsonObject>();
            hop["ordinal"] = ordinal++;
            bool resolved = it->fromNode.length() > 0 && it->toNode.length() > 0 &&
                            it->fromNode != "UNKNOWN" && !it->toNode.startsWith("RXT_NODE_");
            hop["identity_status"] = resolved ? "resolved" : "unresolved";
            if (resolved) {
                hop["tx"] = it->fromNode;
                hop["rx"] = it->toNode;
            }
            hop["has_data"] = true;
            hop["rssi_dbm"] = it->rssi;
            hop["snr_db"] = it->snr;
            hop["frequency_error_hz"] = it->fo;
            hop["tth_ms"] = it->tth;
        }
    }

    String line;
    serializeJson(document, line);
    line += '\n';
    if (line.length() > MAX_RECORD_BYTES) return;

    xSemaphoreTake(eventMutex, portMAX_DELAY);
    latestSequence = sequence;
    if (eventQueue.size() >= EVENT_QUEUE_SIZE) eventQueue.erase(eventQueue.begin());
    eventQueue.push_back({sequence, line});
    xSemaphoreGive(eventMutex);
}

void handleStream(AsyncWebServerRequest *request) {
    if (request->hasParam("after")) {
        request->send(400, "application/json",
                      "{\"code\":\"history_resume_unsupported\",\"message\":\"This pilot producer does not support history resume\"}");
        return;
    }

    uint32_t boundary = sequenceSnapshot();
    std::shared_ptr<StreamState> state = std::make_shared<StreamState>();
    state->nextSequence = boundary + 1;

    AsyncWebServerResponse *response = request->beginChunkedResponse(
        "application/x-ndjson; charset=utf-8",
        [state, boundary](uint8_t *buffer, size_t maxLen, size_t) -> size_t {
            if (state->pending.length() == 0) {
                if (state->helloPending) {
                    state->pending = buildHello(boundary);
                    state->helloPending = false;
                } else {
                    uint32_t oldest = 0;
                    if (!copyQueuedEvent(state->nextSequence, state->pending, oldest)) {
                        if (state->nextSequence < oldest) return 0; // bounded queue: drop slow client
                        return RESPONSE_TRY_AGAIN;
                    }
                    ++state->nextSequence;
                }
                state->pendingOffset = 0;
            }

            size_t available = state->pending.length() - state->pendingOffset;
            size_t count = std::min(maxLen, available);
            memcpy(buffer, state->pending.c_str() + state->pendingOffset, count);
            state->pendingOffset += count;
            if (state->pendingOffset == state->pending.length()) {
                state->pending = "";
                state->pendingOffset = 0;
            }
            return count;
        });
    response->addHeader("Cache-Control", "no-store");
    response->addHeader("Connection", "keep-alive");
    response->addHeader("X-Accel-Buffering", "no");
    request->send(response);
}

} // namespace APRS_JSON_Utils
