#pragma once

#include <Arduino.h>

namespace DIGI_Utils {
String generateDigipeatedPacket(const String& packet, bool thirdParty);
void processLoRaPacket(const String& packet);
}
