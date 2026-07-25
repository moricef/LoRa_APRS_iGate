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

#include "sd_utils.h"

#ifdef HAS_SD_LOG

#include <SPI.h>
#include <SD.h>
#include "utils.h"


#define SD_LOG_FILE     "/aprs_rx.csv"
#define SD_LOG_OLD_FILE "/aprs_rx.old"
#define SD_LOG_MAX_SIZE 4194304     // 4MB, then rotated to SD_LOG_OLD_FILE


namespace SD_Utils {

    SPIClass sdSPI(HSPI);

    bool    cardReady   = false;

    bool    entryOpen   = false;
    String  entryPacket = "";
    String  entryDecision;
    uint32_t entryTime  = 0;
    int     entryRssi   = 0;
    float   entrySnr    = 0.0;
    int     entryFreqErr = 0;

    void writeLine(const String& line) {
        if (!cardReady) return;
        File logFile = SD.open(SD_LOG_FILE, FILE_APPEND);
        if (!logFile) {
            cardReady = false;                                  // card pulled out or gone bad, stop trying
            Utils::println("SD Log: write failed, logging disabled");
            return;
        }
        logFile.println(line);
        logFile.close();
    }

    void rotateIfNeeded() {
        File logFile = SD.open(SD_LOG_FILE, FILE_READ);
        if (!logFile) return;
        const size_t logSize = logFile.size();
        logFile.close();
        if (logSize < SD_LOG_MAX_SIZE) return;
        if (SD.exists(SD_LOG_OLD_FILE)) SD.remove(SD_LOG_OLD_FILE);
        SD.rename(SD_LOG_FILE, SD_LOG_OLD_FILE);
    }

    void setup() {
        sdSPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
        if (!SD.begin(SD_CS_PIN, sdSPI)) {
            Utils::println("init : SD Card       ...     not found! (no packet logging)");
            return;
        }
        cardReady = true;
        rotateIfNeeded();
        if (!SD.exists(SD_LOG_FILE)) {
            writeLine("# t_ms,decision,rssi_dbm,snr_db,ferr_hz,tnc2");
        }
        writeLine("# boot");
        Utils::println("init : SD Card       ...     done!    (logging to " + String(SD_LOG_FILE) + ")");
    }

    void beginEntry(const String& tnc2Packet, const int rssi, const float snr, const int freqError) {
        if (!cardReady) return;
        if (entryOpen) endEntry();                              // previous frame was never closed
        entryOpen       = true;
        entryTime       = millis();
        entryPacket     = tnc2Packet;
        entryDecision   = "DROP";
        entryRssi       = rssi;
        entrySnr        = snr;
        entryFreqErr    = freqError;
    }

    void setDecision(const char* decision) {
        if (!cardReady || !entryOpen) return;
        entryDecision = decision;
    }

    void endEntry() {
        if (!cardReady || !entryOpen) return;
        entryOpen = false;
        entryPacket.replace("\r", "");
        entryPacket.replace("\n", "");

        char header[64];
        snprintf(header, sizeof(header), "%lu,%s,%d,%.2f,%d,", (unsigned long)entryTime, entryDecision.c_str(), entryRssi, entrySnr, entryFreqErr);
        writeLine(String(header) + entryPacket);
        entryPacket = "";
    }

    void logCRC(const int rssi, const float snr, const int freqError) {
        if (!cardReady) return;
        char line[64];
        snprintf(line, sizeof(line), "%lu,CRC,%d,%.2f,%d,", (unsigned long)millis(), rssi, snr, freqError);
        writeLine(String(line));
    }

}

#endif
