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

#include "station_utils.h"
#include "battery_utils.h"
#include "aprs_is_utils.h"
#include "configuration.h"
#include "lora_utils.h"
#include "display.h"
#include "packet_dedup.h"
#include "aprs_return_route.h"
#include "utils.h"
#include <string>
#include <vector>


extern Configuration            Config;
extern String                   fourthLine;
extern bool                     shouldSleepLowVoltage;

std::vector<LastHeardStation>   lastHeardStations;
std::vector<String>             blacklist;
std::vector<String>             managers;
std::vector<LastHeardStation>   lastHeardObjects;

struct OutputPacketBuffer {
    String                    packet;
    bool                      isBeacon;
    LoRa_Utils::RxtRxContext  rxtContext;
    OutputPacketBuffer(const String& p, bool b, bool r)
        : packet(p), isBeacon(b), rxtContext(r ? LoRa_Utils::captureRxtRxContext()
                                               : LoRa_Utils::RxtRxContext{false, 0, 0.0f, 0, 0}) {}
};
std::vector<OutputPacketBuffer> outputPacketBuffer;

PACKET_DEDUP::Cache packetDedupCache;
static_assert(STATION_Utils::DEDUP_DIGI == PACKET_DEDUP::DIGI,
              "digipeater de-duplication destination mismatch");
static_assert(STATION_Utils::DEDUP_RETURN_ROUTE == PACKET_DEDUP::RETURN_ROUTE,
              "return-route de-duplication destination mismatch");

bool saveNewDigiEcoModeConfig   = false;
bool packetIsBeacon             = false;


namespace STATION_Utils {

    std::vector<String> loadCallsignList(const String& list) {
        std::vector<String> loadedList;
        int start       = 0;
        int listLength  = list.length();

        while (start < listLength) {
            while (start < listLength && list[start] == ' ') start++;  // avoid blank spaces
            if (start >= listLength) break;

            int end = start;
            while (end < listLength && list[end] != ' ') end++;         // find another blank space or reach listLength

            loadedList.emplace_back(list.substring(start, end));
            start = end + 1;                                            // keep on searching if listLength not reached
        }
        return loadedList;
    }

    void loadBlacklistAndManagers() {
        blacklist   = loadCallsignList(Config.blacklist);
        managers    = loadCallsignList(Config.remoteManagement.managers);
    }

    bool checkCallsignList(const std::vector<String>& list, const String& callsign) {
        for (size_t i = 0; i < list.size(); i++) {
            int wildcardIndex = list[i].indexOf("*");
            if (wildcardIndex >= 0) {
                if (wildcardIndex >= 2 && callsign.length() >= wildcardIndex && strncmp(callsign.c_str(), list[i].c_str(), wildcardIndex) == 0) {
                    return true;
                }
            } else {
                if (list[i] == callsign) return true;
            }
        }
        return false;
    }

    bool isBlacklisted(const String& callsign) {
        return checkCallsignList(blacklist, callsign);
    }

    bool isManager(const String& callsign) {
        return checkCallsignList(managers, callsign);
    }

    void cleanObjectsHeard() {
        for (auto it = lastHeardObjects.begin(); it != lastHeardObjects.end(); ) {
            if (millis() - it->lastHeardTime >= 9.75 * 60 * 1000) { // 9.75 = 9min 45secs
                it = lastHeardObjects.erase(it);    // erase() returns the next valid iterator
            } else {
                ++it;                               // Only increment if not erasing
            }
        }
    }

    bool checkObjectTime(const String& packet) {
        cleanObjectsHeard();

        int objectIDIndex = packet.indexOf(":;");
        String object = packet.substring(objectIDIndex + 2, objectIDIndex + 11);
        object.trim();

        for (int i = 0; i < lastHeardObjects.size(); i++) {                 // Check if i should Tx object
            if (lastHeardObjects[i].station == object) return false;
        }
        lastHeardObjects.emplace_back(LastHeardStation{millis(), object, "", false});  // Add new object and Tx
        return true;
    }

    void deleteNotHeard() {
        uint32_t currentTime    = millis();
        uint32_t timeout        = Config.rememberStationTime * 60UL * 1000UL;

        for (int i = lastHeardStations.size() - 1; i >= 0; i--) {
            if (currentTime - lastHeardStations[i].lastHeardTime >= timeout) {
                lastHeardStations.erase(lastHeardStations.begin() + i);
            }
        }
    }

    void updateLastHeard(const String& station) {
        deleteNotHeard();
        uint32_t currentTime = millis();
        for (size_t i = 0; i < lastHeardStations.size(); i++) {
            if (lastHeardStations[i].station == station) {
                lastHeardStations[i].lastHeardTime = currentTime;
                Utils::showActiveStations();
                return;
            }
        }
        lastHeardStations.emplace_back(LastHeardStation{currentTime, station, "", false});
        Utils::showActiveStations();
    }

    bool wasHeard(const String& station) {
        deleteNotHeard();
        for (size_t i = 0; i < lastHeardStations.size(); i++) {
            if (lastHeardStations[i].station == station) {
                Utils::println(" ---> Listened Station");
                return true;
            }
        }
        Utils::println(" ---> Station not Heard in " + String(Config.rememberStationTime) + " min: No Tx");
        return false;
    }

    void observeReturnPath(const String& packet) {
        const int gt = packet.indexOf('>');
        const int colon = packet.indexOf(':');
        if (gt <= 0 || colon <= gt + 1 || colon + 1 >= static_cast<int>(packet.length())) return;
        // The outer RF path of an Internet third-party message does not
        // describe a route to its inner sender.
        if (packet[colon + 1] == '}') return;
        const String header = packet.substring(0, colon);
        if (header.indexOf("TCPIP") != -1 || header.indexOf("TCPXX") != -1) return;
        const String sender = packet.substring(0, gt);
        if (sender == Config.callsign || sender == Config.tacticalCallsign ||
            !Utils::callsignIsValid(sender)) return;
        updateLastHeard(sender);
        const int comma = packet.indexOf(',', gt + 1);
        const String aprsDestination = packet.substring(gt + 1,
            comma != -1 && comma < colon ? comma : colon);
        // First RF copy wins, independently of upload and relay decisions.
        if (claimPacketDestination(sender, packet.substring(colon + 1),
                                   DEDUP_RETURN_ROUTE, aprsDestination)) {
            learnReturnPath(sender, packet);
        }
    }

    void learnReturnPath(const String& station, const String& packet) {
        std::vector<std::string> aliases;
        aliases.emplace_back("WIDE");
        unsigned int start = 0;
        while (start < Config.digi.regionalAliases.length()) {
            while (start < Config.digi.regionalAliases.length() &&
                   (Config.digi.regionalAliases[start] == ' ' ||
                    Config.digi.regionalAliases[start] == ',')) start++;
            if (start >= Config.digi.regionalAliases.length()) break;
            int end = start;
            while (end < static_cast<int>(Config.digi.regionalAliases.length()) &&
                   Config.digi.regionalAliases[end] != ' ' &&
                   Config.digi.regionalAliases[end] != ',') end++;
            const String alias = Config.digi.regionalAliases.substring(start, end);
            if (!alias.equalsIgnoreCase("WIDE")) aliases.emplace_back(alias.c_str());
            start = end + 1;
        }

        const APRS_RETURN_ROUTE::Result result = APRS_RETURN_ROUTE::derive(
            std::string(packet.c_str(), packet.length()),
            std::string(Config.callsign.c_str(), Config.callsign.length()),
            std::string(Config.tacticalCallsign.c_str(), Config.tacticalCallsign.length()), aliases);
        if (!result.valid) {
            Utils::println("[RETURN-PATH] Invalid RF path for " + station);
            return;
        }

        deleteNotHeard();
        const uint32_t currentTime = millis();
        for (LastHeardStation& entry : lastHeardStations) {
            if (entry.station != station) continue;
            entry.lastHeardTime = currentTime;
            entry.returnPath = result.path.c_str();
            entry.returnPathKnown = true;
            Utils::println("[RETURN-PATH] Learned " + station + " via " +
                           (entry.returnPath == "" ? String("DIRECT") : entry.returnPath));
            return;
        }

        const String path(result.path.c_str());
        lastHeardStations.emplace_back(LastHeardStation{currentTime, station, path, true});
        Utils::println("[RETURN-PATH] Learned " + station + " via " +
                       (path == "" ? String("DIRECT") : path));
        Utils::showActiveStations();
    }

    bool getReturnPath(const String& station, String& path) {
        deleteNotHeard();
        for (const LastHeardStation& entry : lastHeardStations) {
            if (entry.station != station || !entry.returnPathKnown) continue;
            path = entry.returnPath;
            return true;
        }
        path = "";
        return false;
    }

    String localReplyHeader(const String& source, const String& recipient, bool thirdParty) {
        String path;
        APRS_RETURN_ROUTE::Result route;
        // Internet senders need the existing gateway-return behavior; do not
        // substitute a possibly stale RF route to the encapsulated sender.
        route.valid = !thirdParty && getReturnPath(recipient, path);
        route.path = path.c_str();
        const std::string header = APRS_RETURN_ROUTE::buildLocalReplyHeader(
            source.c_str(), route, Config.beacon.path.c_str(), !thirdParty);
        return String(header.c_str());
    }

    bool claimPacketDestination(const String& station, const String& information,
                                uint8_t destination, const String& aprsDestination) {
        String baseStation = station;
        int gtIdx = baseStation.indexOf('>');
        if (gtIdx != -1) {
            baseStation = baseStation.substring(0, gtIdx);
        }
        baseStation.trim();
        const std::string source(baseStation.c_str(), baseStation.length());
        const std::string payload(information.c_str(), information.length());
        return packetDedupCache.claim(source, payload, destination, millis(),
            std::string(aprsDestination.c_str(), aprsDestination.length()));
    }

    void processOutputPacketBufferUltraEcoMode() {
        size_t currentIndex = 0;
        while (currentIndex < outputPacketBuffer.size()) {                  // this sends all packets from output buffer
            delay(3000);                                                    // and cleans buffer to avoid sending packets with time offset
            if (outputPacketBuffer[currentIndex].isBeacon) packetIsBeacon = true;
            const LoRa_Utils::RxtRxContext* rxtContext = outputPacketBuffer[currentIndex].rxtContext.valid
                ? &outputPacketBuffer[currentIndex].rxtContext : nullptr;
            LoRa_Utils::sendNewPacket(outputPacketBuffer[currentIndex].packet, rxtContext);    // next time it wakes up
            if (outputPacketBuffer[currentIndex].isBeacon) packetIsBeacon = false;
            currentIndex++;
        }
        outputPacketBuffer.clear();
        //
        if (saveNewDigiEcoModeConfig) {
            Config.writeFile();
            delay(1000);
            displayToggle(false);
            ESP.restart();
        }
        //
    }

    void processOutputPacketBuffer() {
        if (outputPacketBuffer.size() > 0) {
            if (outputPacketBuffer[0].isBeacon) packetIsBeacon = true;
            const LoRa_Utils::RxtRxContext* rxtContext = outputPacketBuffer[0].rxtContext.valid
                ? &outputPacketBuffer[0].rxtContext : nullptr;
            LoRa_Utils::sendNewPacket(outputPacketBuffer[0].packet, rxtContext);
            if (outputPacketBuffer[0].isBeacon) packetIsBeacon = false;
            outputPacketBuffer.erase(outputPacketBuffer.begin());
        }
        if (shouldSleepLowVoltage) {
            while (outputPacketBuffer.size() > 0) {
                if (outputPacketBuffer[0].isBeacon) packetIsBeacon = true;
                const LoRa_Utils::RxtRxContext* rxtContext = outputPacketBuffer[0].rxtContext.valid
                    ? &outputPacketBuffer[0].rxtContext : nullptr;
                LoRa_Utils::sendNewPacket(outputPacketBuffer[0].packet, rxtContext);
                if (outputPacketBuffer[0].isBeacon) packetIsBeacon = false;
                outputPacketBuffer.erase(outputPacketBuffer.begin());
                delay(4000);
            }
        }
        if (saveNewDigiEcoModeConfig) {
            Config.writeFile();
            delay(1000);
            displayToggle(false);
            ESP.restart();
        }
    }

    void addToOutputPacketBuffer(const String& packet, bool flag, bool eligibleForRxt) {
        // Callers use an empty string to mean "no response/no packet". Never
        // let that sentinel reach the LoRa transmitter.
        if (packet.length() == 0) return;
        outputPacketBuffer.emplace_back(OutputPacketBuffer{packet, flag, eligibleForRxt});
    }

}
