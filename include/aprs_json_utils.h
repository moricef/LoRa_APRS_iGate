/* Copyright (C) 2026 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate and is distributed under GPLv3.
 */

#ifndef APRS_JSON_UTILS_H_
#define APRS_JSON_UTILS_H_

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <vector>

#include "lora_utils.h"

namespace APRS_JSON_Utils {

    void setup();
    void handleStream(AsyncWebServerRequest *request);
    void handleEvents(AsyncWebServerRequest *request);
    void recordRx(const String& rfPacket,
                  const LoRa_Utils::RxtRxContext& localRx,
                  const String& rawRxtField,
                  const std::vector<LoRa_Utils::RxtHopMetric>& hopMetrics);

}

#endif
