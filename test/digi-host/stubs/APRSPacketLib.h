#pragma once

#include <Arduino.h>

struct APRSPacket {
    String sender;
    String header;
    int type = 0;
};

namespace APRSPacketLib {
String checkForStartingBytes(const String& packet);
}
