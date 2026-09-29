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
 *      t_ms,event,rssi_dbm,snr_db,ferr_hz,tth_ms,rxt_rx_hex,rxt_tx_hex,tnc2
 *  t_ms is relative to boot (millis()), the frame is the last field so its
 *  commas need no quoting. Decisions:
 *      RELAY   digipeated (queued in the output buffer)
 *      DUP     already seen in the 25s dedup buffer
 *      PATH    no usable WIDEn-N path for the current digi mode
 *      BLACK   sender is blacklisted
 *      DROP    dropped for any other reason (own packet, NOGATE, invalid
 *              callsign, query answered, digi disabled)
 *      CRC     CRC error, no frame decoded (edge of coverage)
 *      RXT_TX  successfully transmitted relay carrying the local RXT tuple
 */

namespace SD_Utils {

    #ifdef HAS_SD_LOG

        // Remote download of the log (current file, or the rotated .old one).
        constexpr size_t READ_BUSY = SIZE_MAX;   // readLog(): log being written, try again
        bool    logFileSize(const bool previous, size_t& size);
        size_t  readLog(const bool previous, const size_t offset, uint8_t* buffer, const size_t length);

        void setup();
        void beginEntry(const String& tnc2Packet, const int rssi, const float snr,
                        const int freqError, const String& receivedRxt = "");
        void setDecision(const char* decision);
        void endEntry();
        void logCRC(const int rssi, const float snr, const int freqError);
        void logRxtTx(const String& tnc2Packet, const int rssi, const float snr,
                      const int freqError, const unsigned long tth, const String& tuple);

    #else

        constexpr size_t READ_BUSY = SIZE_MAX;
        inline bool logFileSize(const bool, size_t&) { return false; }
        inline size_t readLog(const bool, const size_t, uint8_t*, const size_t) { return 0; }
        inline void setup() {}
        inline void beginEntry(const String&, const int, const float, const int, const String& = "") {}
        inline void setDecision(const char*) {}
        inline void endEntry() {}
        inline void logCRC(const int, const float, const int) {}
        inline void logRxtTx(const String&, const int, const float, const int,
                             const unsigned long, const String&) {}

    #endif

}

#endif
