/* Copyright (C) 2026 Ricardo Guzman - CA2RXU
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

#include <APRSPacketLib.h>
#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include "telemetry_utils.h"
#include "aprs_telemetry_persistence.h"
#include "aprs_telemetry_rx.h"
#include "aprs_is_utils.h"
#include "configuration.h"
#include "station_utils.h"
#include "battery_utils.h"
#include "lora_utils.h"
#include "wx_utils.h"
#include "display.h"


extern      Configuration       Config;
extern      APRS_Telemetry_RX::Store aprsTelemetryStore;

uint16_t    telemetryCounter    = 0;
uint32_t    telemetryEUPTime    = 0;
bool        sendEUP             = false;    // Equations Units Parameters

static uint32_t rxCount             = 0;    // frames heard (valid LoRa APRS) since last telemetry
static uint32_t relayCount          = 0;    // frames digipeated since last telemetry
static uint32_t dropCount           = 0;    // frames rejected by the digi (DUP, PATH, BLACK, self, NOGATE, etc.) since last telemetry
static uint32_t telemetryWindowStart = 0;   // millis() at the previous encoded telemetry report
static constexpr uint32_t telemetryMetadataInterval = 6UL * 60UL * 60UL * 1000UL;
static constexpr uint16_t telemetrySequenceModulus = 8281;
static constexpr uint16_t telemetrySequenceReservation = 64;
static uint16_t telemetrySequencesRemaining = 0;

static void reserveTelemetrySequences() {
    const uint16_t fallback = static_cast<uint16_t>(random(telemetrySequenceModulus));
    uint16_t first = fallback;

    Preferences preferences;
    if (preferences.begin("aprs-tlm", false)) {
        first = preferences.getUShort("seq-next", fallback) % telemetrySequenceModulus;
        const uint16_t next = static_cast<uint16_t>(
            (first + telemetrySequenceReservation) % telemetrySequenceModulus);
        preferences.putUShort("seq-next", next);
        preferences.end();
    }

    telemetryCounter = first;
    telemetrySequencesRemaining = telemetrySequenceReservation;
}

static uint16_t nextTelemetrySequence() {
    if (telemetrySequencesRemaining == 0) reserveTelemetrySequences();
    const uint16_t sequence = telemetryCounter;
    telemetryCounter = static_cast<uint16_t>((telemetryCounter + 1) % telemetrySequenceModulus);
    --telemetrySequencesRemaining;
    return sequence;
}


namespace TELEMETRY_Utils {

    String joinWithCommas(const std::vector<String>& items) {
        String result;
        for (size_t i = 0; i < items.size(); ++i) {
            result += items[i];
            if (i < items.size() - 1) result += ",";
        }
        return result;
    }

    std::vector<String> getEquationCoefficients() {
        std::vector<String> coefficients;
        if (Config.battery.sendInternalVoltage) coefficients.push_back("0,0.01,0");
        if (Config.battery.sendExternalVoltage) coefficients.push_back("0,0.02,0");
        coefficients.push_back("0,1,0");
        coefficients.push_back("0,1,0");
        coefficients.push_back("0,1,0");
        return coefficients;
    }

    std::vector<String> getUnitLabels() {
        std::vector<String> labels;
        if (Config.battery.sendInternalVoltage) labels.push_back("VDC");
        if (Config.battery.sendExternalVoltage) labels.push_back("VDC");
        labels.push_back("pkt/h");
        labels.push_back("pkt/h");
        labels.push_back("pkt/h");
        return labels;
    }

    std::vector<String> getParameterNames() {
        std::vector<String> names;
        if (Config.battery.sendInternalVoltage) names.push_back("V_Batt");
        if (Config.battery.sendExternalVoltage) names.push_back("V_Ext");
        names.push_back("RX_rate");
        names.push_back("RelRate");
        names.push_back("DrpRate");
        return names;
    }

    void sendBaseTelemetryPacket(const String& prefix, const std::vector<String>& values) {
        String packet           = prefix + joinWithCommas(values);
        String currentCallsign  = (Config.tacticalCallsign != "") ? Config.tacticalCallsign : Config.callsign;
        if (Config.beacon.sendViaAPRSIS) {
            String baseAPRSISTelemetryPacket = APRSPacketLib::generateMessagePacket(currentCallsign, "APLRG1", "TCPIP,qAC", currentCallsign, packet);
            aprsTelemetryStore.ingest(baseAPRSISTelemetryPacket.c_str(), "", millis());
            APRS_Telemetry_Persistence::remember(baseAPRSISTelemetryPacket);
            #ifdef HAS_A7670
                A7670_Utils::uploadToAPRSIS(baseAPRSISTelemetryPacket);
            #else
                APRS_IS_Utils::upload(baseAPRSISTelemetryPacket);
            #endif
            delay(300);
        } else if (Config.beacon.sendViaRF) {
            String baseRFTelemetryPacket = APRSPacketLib::generateMessagePacket(currentCallsign, "APLRG1", Config.beacon.path, currentCallsign, packet);
            aprsTelemetryStore.ingest(baseRFTelemetryPacket.c_str(), "", millis());
            APRS_Telemetry_Persistence::remember(baseRFTelemetryPacket);
            // Self-originated content has no receive context, so no RXT tuple
            // is attached by sendNewPacket().
            LoRa_Utils::sendNewPacket(baseRFTelemetryPacket);
            delay(3000);
        }
    }

    void sendEquationsUnitsParameters() {
        sendBaseTelemetryPacket("EQNS.", getEquationCoefficients());
        sendBaseTelemetryPacket("UNIT.", getUnitLabels());
        sendBaseTelemetryPacket("PARM.", getParameterNames());
        sendEUP = false;
    }

    String generateEncodedTelemetryBytes(float value, bool counterBytes, byte telemetryType) {
        String encodedBytes;
        int tempValue;

        if (counterBytes) {
            tempValue = value;
        } else {
            switch (telemetryType) {
                case 0: tempValue = value * 100; break;         // Internal voltage (0-4,2V), Humidity, Gas calculation
                case 1: tempValue = (value * 100) / 2; break;   // External voltage calculation (0-15V)
                case 2: tempValue = (value * 10) + 500; break;  // Temperature
                case 3: tempValue = (value * 8); break;         // Pressure
                default: tempValue = value; break;
            }
        }

        int firstByte   = tempValue / 91;
        tempValue       -= firstByte * 91;

        encodedBytes    = char(firstByte + 33);
        encodedBytes    += char(tempValue + 33);
        return encodedBytes;
    }

    uint16_t counterRatePerHour(uint32_t count, uint32_t elapsedMs) {
        if (elapsedMs == 0) return 0;
        uint64_t rate = (static_cast<uint64_t>(count) * 3600000ULL + elapsedMs / 2) / elapsedMs;
        return static_cast<uint16_t>((rate > 8280ULL) ? 8280ULL : rate);
    }

    String generateEncodedTelemetry() {
        const uint32_t now       = millis();
        const uint32_t elapsedMs = now - telemetryWindowStart;
        const uint16_t rxRate    = counterRatePerHour(rxCount, elapsedMs);
        const uint16_t relayRate = counterRatePerHour(relayCount, elapsedMs);
        const uint16_t dropRate  = counterRatePerHour(dropCount, elapsedMs);

        String telemetry = "|";
        telemetry += generateEncodedTelemetryBytes(nextTelemetrySequence(), true, 0);
        if (Config.battery.sendInternalVoltage) telemetry += generateEncodedTelemetryBytes(BATTERY_Utils::checkInternalVoltage(), false, 0);
        if (Config.battery.sendExternalVoltage) telemetry += generateEncodedTelemetryBytes(BATTERY_Utils::checkExternalVoltage(), false, Config.battery.useExternalI2CSensor ? 0 : 1);
        telemetry += generateEncodedTelemetryBytes(rxRate,    true, 0);
        telemetry += generateEncodedTelemetryBytes(relayRate, true, 0);
        telemetry += generateEncodedTelemetryBytes(dropRate,  true, 0);
        rxCount = relayCount = dropCount = 0;
        telemetryWindowStart = now;
        telemetry += "|";
        return telemetry;
    }

    void incRx()    { if (rxCount    < UINT32_MAX) rxCount++; }
    void incRelay() { if (relayCount < UINT32_MAX) relayCount++; }
    void incDrop()  { if (dropCount  < UINT32_MAX) dropCount++; }

    void checkEUPInterval() {
        if (telemetryEUPTime == 0 || millis() - telemetryEUPTime >= telemetryMetadataInterval) {
            sendEUP = true;
            telemetryEUPTime = millis();
        }
    }

}
