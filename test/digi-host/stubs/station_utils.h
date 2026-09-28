#pragma once

#include <Arduino.h>

namespace STATION_Utils {
constexpr uint8_t DEDUP_DIGI = 1U << 0;
bool claimPacketDestination(const String& station, const String& information,
                            uint8_t destination, const String& aprsDestination);
void updateLastHeard(const String& station);
void addToOutputPacketBuffer(const String& packet, bool flag = false,
                             bool eligibleForRxt = false);
}
