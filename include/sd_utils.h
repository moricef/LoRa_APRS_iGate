/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate.
 *
 * LoRa APRS iGate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LoRa APRS iGate is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LoRa APRS iGate. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SD_UTILS_H_
#define SD_UTILS_H_

#include <Arduino.h>
#include "board_pinout.h"

/*  One CSV line per received frame on the microSD card:
 *      t_ms,decision,rssi_dbm,snr_db,ferr_hz,tnc2
 *  t_ms is relative to boot (millis()), the frame is the last field so its
 *  commas need no quoting. Decisions:
 *      RELAY   digipeated (queued in the output buffer)
 *      DUP     already seen in the 25s dedup buffer
 *      PATH    no usable WIDEn-N path for the current digi mode
 *      BLACK   sender is blacklisted
 *      DROP    dropped for any other reason (own packet, NOGATE, invalid
 *              callsign, query answered, digi disabled)
 *      CRC     CRC error, no frame decoded (edge of coverage)
 */

namespace SD_Utils {

    #ifdef HAS_SD_LOG

        void setup();
        void beginEntry(const String& tnc2Packet, const int rssi, const float snr, const int freqError);
        void setDecision(const char* decision);
        void endEntry();
        void logCRC(const int rssi, const float snr, const int freqError);

    #else

        inline void setup() {}
        inline void beginEntry(const String&, const int, const float, const int) {}
        inline void setDecision(const char*) {}
        inline void endEntry() {}
        inline void logCRC(const int, const float, const int) {}

    #endif

}

#endif
