#pragma once

#include <Arduino.h>

namespace APRS_IS_Utils {
bool processReceivedLoRaMessage(const String& sender,
                                const String& addresseeAndMessage,
                                bool thirdParty,
                                const String& responder);
}
