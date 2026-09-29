#pragma once

#include <Arduino.h>
#include <APRSPacketLib.h>

namespace Utils {
bool callsignIsValid(const String& callsign);
void updateLoRaPacketDisplayInfo(APRSPacket& aprsPacket, uint8_t packetType);
void println(const String& message);
}
