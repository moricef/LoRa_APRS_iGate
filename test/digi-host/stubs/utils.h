#pragma once

#include <Arduino.h>

namespace Utils {
bool callsignIsValid(const String& callsign);
void typeOfPacket(const String& packet, uint8_t type);
}
