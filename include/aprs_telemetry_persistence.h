#ifndef APRS_TELEMETRY_PERSISTENCE_H_
#define APRS_TELEMETRY_PERSISTENCE_H_

#include <Arduino.h>
#include "aprs_telemetry_rx.h"

namespace APRS_Telemetry_Persistence {

void load(APRS_Telemetry_RX::Store& store);
bool remember(const String& packet);

} // namespace APRS_Telemetry_Persistence

#endif
