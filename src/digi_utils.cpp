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

#include <WiFi.h>
#include "configuration.h"
#include "station_utils.h"
#include "aprs_is_utils.h"
#include "digi_utils.h"
#include "wifi_utils.h"
#include "lora_utils.h"
#include "sd_utils.h"
#include "telemetry_utils.h"
#include "display.h"
#include "utils.h"

extern int      rssi;
extern float    snr;
extern int      freqOffset;
extern Configuration    Config;
extern uint32_t         lastScreenOn;
extern String           iGateBeaconPacket;
extern String           firstLine;
extern String           secondLine;
extern String           thirdLine;
extern String           fourthLine;
extern String           fifthLine;
extern String           sixthLine;
extern String           seventhLine;
extern bool             backupDigiMode;


namespace DIGI_Utils {

    struct RegionalHop {
        bool found = false;
        int tokenStart = -1;
        int tokenEnd = -1;
        String alias;
        int total = 0;
        int remaining = 0;
    };

    static bool isAliasCharacter(char character) {
        return (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9');
    }

    static bool isAliasSeparator(char character) {
        return character == ' ' || character == ',' || character == '\t' ||
               character == '\r' || character == '\n';
    }

    static int pathTokenIndex(const String& path, const String& expected) {
        unsigned int start = 0;
        while (start < path.length()) {
            int end = path.indexOf(',', start);
            if (end == -1) end = path.length();
            if (path.substring(start, end) == expected) return start;
            start = end + 1;
        }
        return -1;
    }

    static bool pathContainsCallsign(const String& path, const String& callsign) {
        unsigned int start = 0;
        while (start < path.length()) {
            int end = path.indexOf(',', start);
            if (end == -1) end = path.length();
            String token = path.substring(start, end);
            if (token.endsWith("*")) token = token.substring(0, token.length() - 1);
            if (token.equalsIgnoreCase(callsign)) return true;
            start = end + 1;
        }
        return false;
    }

    static bool isDecimal(const String& text) {
        if (text.length() == 0) return false;
        for (unsigned int i = 0; i < text.length(); i++) {
            if (text[i] < '0' || text[i] > '9') return false;
        }
        return true;
    }

    static bool parseRegionalToken(const String& token, const String& alias,
                                   int& total, int& remaining) {
        if (alias.length() == 0 || alias.length() > 5 || token.endsWith("*")) return false;
        for (unsigned int i = 0; i < alias.length(); i++) {
            if (!isAliasCharacter(alias[i])) return false;
        }

        if (!token.startsWith(alias)) return false;
        int dash = token.indexOf('-', alias.length() + 1);
        if (dash == -1 || token.indexOf('-', dash + 1) != -1) return false;

        String totalText = token.substring(alias.length(), dash);
        String remainingText = token.substring(dash + 1);
        if (!isDecimal(totalText) || !isDecimal(remainingText)) return false;
        if (dash > 6) return false;  // AX.25 address field before the SSID.

        total = totalText.toInt();
        remaining = remainingText.toInt();
        return total >= 1 && total <= Config.digi.regionalMaxHops &&
               remaining >= 1 && remaining <= total;
    }

    static bool matchConfiguredRegionalToken(const String& token, RegionalHop& hop) {
        unsigned int start = 0;
        const String& aliases = Config.digi.regionalAliases;
        while (start < aliases.length()) {
            while (start < aliases.length() && isAliasSeparator(aliases[start])) start++;
            if (start >= aliases.length()) break;

            int end = start;
            while (end < static_cast<int>(aliases.length()) &&
                   !isAliasSeparator(aliases[end])) end++;
            String alias = aliases.substring(start, end);
            alias.toUpperCase();

            int total = 0;
            int remaining = 0;
            if (parseRegionalToken(token, alias, total, remaining)) {
                hop.found = true;
                hop.alias = alias;
                hop.total = total;
                hop.remaining = remaining;
                return true;
            }
            start = end + 1;
        }
        return false;
    }

    static RegionalHop findRegionalHop(const String& path) {
        RegionalHop hop;
        unsigned int start = 0;
        while (start < path.length()) {
            int end = path.indexOf(',', start);
            if (end == -1) end = path.length();
            String token = path.substring(start, end);
            if (matchConfiguredRegionalToken(token, hop)) {
                hop.tokenStart = start;
                hop.tokenEnd = end;
                return hop;
            }
            start = end + 1;
        }
        return hop;
    }

    static String consumeRegionalHop(const String& path, const String& stationCallsign) {
        RegionalHop hop = findRegionalHop(path);
        if (!hop.found) return "";

        String replacement = stationCallsign + "*";
        if (hop.remaining > 1) {
            replacement += "," + hop.alias + String(hop.total) + "-" + String(hop.remaining - 1);
        }
        return path.substring(0, hop.tokenStart) + replacement + path.substring(hop.tokenEnd);
    }

    String cleanPath(String path) {
        String terms[] = {"WIDE1*,", "WIDE2*,"};
        for (String term : terms) {
            int index = path.indexOf(term);
            if (index != -1) path.remove(index, term.length());    // less memory than: tempPath.replace("*", "");
        }
        // TNC2 marks the last used path element with one asterisk. Earlier
        // elements are implicitly used, so never carry their markers into a
        // newly generated hop.
        path.replace("*", "");
        return path;
    }

    String processMode3Path(const String& path, const String& stationCallsign) {
        unsigned int start = 0;
        int tokenIndex = 0;
        int lastStarredIndex = -1;
        int ownTokenIndex = -1;
        int ownTokenEnd = -1;

        while (start < path.length()) {
            int delim = path.indexOf(',', start);
            if (delim == -1) delim = path.length();         // busca todo hasta lograr encontra una coma o el final del string

            String token = path.substring(start, delim);
            bool tokenStar = token.endsWith("*");
            const String tokenCallsign = tokenStar ? token.substring(0, token.length() - 1) : token;
            bool tokenIsOwn = tokenCallsign.equalsIgnoreCase(stationCallsign);

            if (tokenIsOwn) {
                if (tokenStar) return "";                   // already digipeated
                if (ownTokenIndex != -1) return "";          // repeated own identity
                ownTokenIndex = tokenIndex;
                ownTokenEnd = delim;
            }

            if (tokenStar) lastStarredIndex = tokenIndex;
            tokenIndex++;
            start = delim + 1;
        }

        if (ownTokenEnd == -1) return "";
        // In canonical TNC2 only the last used element carries '*'. Our
        // identity must be the first unused path element. The older form with
        // every used element starred remains accepted as well.
        if (lastStarredIndex >= ownTokenIndex) return "";
        if (ownTokenIndex > 0 && lastStarredIndex != ownTokenIndex - 1) return "";
        String tempPacket = cleanPath(path.substring(0, ownTokenEnd));
        return tempPacket + "*" + path.substring(ownTokenEnd);
    }

    String buildPacket(const String& path, const String& packet, bool thirdParty, bool crossFreq) {
        String stationCallsign  = (Config.tacticalCallsign == "" ? Config.callsign : Config.tacticalCallsign);
        stationCallsign.trim();
        if (stationCallsign == "" || stationCallsign.equalsIgnoreCase("undefined") || stationCallsign.equalsIgnoreCase("null")) {
            return "";
        }
        String suffix           = thirdParty ? ":}" : ":";
        int suffixIndex         = packet.indexOf(suffix);
        String packetToRepeat;
        if (!crossFreq) {
            int digiMode        = Config.digi.mode;
            String tempPath     = path;

            // Explicit source routes are valid in every normal digi mode.
            // This is required for an iGate to reverse the path on which it
            // last heard a station. processMode3Path() also rejects a route
            // already consumed by this digi or one whose preceding hop is not
            // the last used path element.
            if ((digiMode == 1 || digiMode == 2) &&
                pathContainsCallsign(tempPath, stationCallsign)) {
                tempPath = processMode3Path(tempPath, stationCallsign);
                if (tempPath == "") return "";
            } else {
                int wide1Index = pathTokenIndex(tempPath, "WIDE1-1");
                if (wide1Index != -1 && (digiMode == 1 || digiMode == 2)) {                 // WIDE1-1
                if (tempPath.indexOf("*") != -1 ) return "";                                // "*" shouldn't be in WIDE1-1 (only) type of packet
                tempPath = tempPath.substring(0, wide1Index) + stationCallsign + "*" +
                           tempPath.substring(wide1Index + 7);
                } else if (digiMode == 2) {                                                 // Configured regional alias
                    tempPath = cleanPath(path);
                    tempPath = consumeRegionalHop(tempPath, stationCallsign);
                    if (tempPath == "") return "";
                }
            }
            if (digiMode == 3) {                                                            // Repeat if station callsign is in path (free to repeat).
                tempPath = processMode3Path(tempPath, stationCallsign);
                if (tempPath == "") return "";
            }
            packetToRepeat = packet.substring(0, packet.indexOf(",") + 1);
            packetToRepeat += tempPath;
        } else {   // CrossFreq Digipeater
            packetToRepeat = cleanPath(packet.substring(0, suffixIndex));
            if (pathContainsCallsign(path, stationCallsign)) return "";                  // stationCallsign shouldn't be in path
            packetToRepeat += ",";
            packetToRepeat += stationCallsign;
            packetToRepeat += "*";
        }
        packetToRepeat += APRS_IS_Utils::checkForStartingBytes(packet.substring(suffixIndex));
        return packetToRepeat;
    }

    String generateDigipeatedPacket(const String& packet, bool thirdParty){
        String temp;
        if (thirdParty) {   // only header is used
            const String& header = packet.substring(0, packet.indexOf(":}"));
            temp = header.substring(header.indexOf(">") + 1);
        } else {
            temp = packet.substring(packet.indexOf(">") + 1, packet.indexOf(":"));
        }
        int commaIndex      = temp.indexOf(",");
        int digiMode        = Config.digi.mode;
        bool crossFreq      = abs(Config.loramodule.txFreq - Config.loramodule.rxFreq) >= 125000;   // CrossFreq Digi

        if (commaIndex > 2) {   // "path" found
            const String& path  = temp.substring(commaIndex + 1);
            if (digiMode == 1 || backupDigiMode) {
                bool hasWide = pathTokenIndex(path, "WIDE1-1") != -1;
                bool hasOwnCall = pathContainsCallsign(path,
                    Config.tacticalCallsign == "" ? Config.callsign : Config.tacticalCallsign);
                if (hasOwnCall || hasWide || crossFreq) {
                    return buildPacket(path, packet, thirdParty, hasOwnCall ? false : !hasWide);
                }
                return "";
            }
            if (digiMode == 2) {
                int wide1Index = pathTokenIndex(path, "WIDE1-1");
                bool hasWide1 = wide1Index != -1;
                bool hasOwnCall = pathContainsCallsign(path,
                    Config.tacticalCallsign == "" ? Config.callsign : Config.tacticalCallsign);
                RegionalHop regionalHop = findRegionalHop(path);

                if (hasOwnCall) return buildPacket(path, packet, thirdParty, false);
                if (hasWide1 && regionalHop.found && regionalHop.tokenStart < wide1Index) return ""; // fill-in must come first

                if (hasWide1 || regionalHop.found) return buildPacket(path, packet, thirdParty, false);

                if (crossFreq) return buildPacket(path, packet, thirdParty, true);                  // CrossFreq (without WIDE)

                return "";
            }
            if (digiMode == 3) {
                String stationCallsign  = (Config.tacticalCallsign == "" ? Config.callsign : Config.tacticalCallsign);
                bool containsOwnCall    = pathContainsCallsign(path, stationCallsign);
                if (containsOwnCall) return buildPacket(path, packet, thirdParty, false);
                return "";
            }
            return "";
        }

        if (commaIndex == -1 && (digiMode == 1 || backupDigiMode || digiMode == 2) && crossFreq) return buildPacket("", packet, thirdParty, true);  // no "path" but is CrossFreq Digi

        return "";
    }

    void processLoRaPacket(const String& packet) {
        if (packet.indexOf("NOGATE") >= 0) { TELEMETRY_Utils::incDrop(); return; }

        bool thirdPartyPacket = false;
        String temp, Sender;
        int firstColonIndex = packet.indexOf(":");
        
        if (firstColonIndex > 5 && firstColonIndex < (packet.length() - 1) && packet[firstColonIndex + 1] == '}' && packet.indexOf("TCPIP") > 0) {   // 3rd Party
            thirdPartyPacket = true;
            temp    = packet.substring(packet.indexOf(":}") + 2);
            Sender  = temp.substring(0, temp.indexOf(">"));
        } else {
            temp    = packet;
            int gtIdx = packet.indexOf('>');
            if (gtIdx != -1) {
                // Extract sender from the absolute beginning of the packet up to '>'
                Sender = packet.substring(0, gtIdx);
            } else {
                return; // Malformed header
            }
        }

        Sender.trim();
        String stationCallsign = Config.tacticalCallsign == "" ? Config.callsign : Config.tacticalCallsign;
        if (Sender == stationCallsign) { TELEMETRY_Utils::incDrop(); return; }          // Avoid listening to self packets
        if (!thirdPartyPacket && Config.tacticalCallsign == "" && !Utils::callsignIsValid(Sender)) { TELEMETRY_Utils::incDrop(); return; }  // No thirdParty + no tactical + invalid callsign

        STATION_Utils::updateLastHeard(Sender);
        Utils::typeOfPacket(temp, 2);              // Digi
        bool queryMessage                   = false;
        int doubleColonIndex                = temp.indexOf("::");
        if (doubleColonIndex > 10) {                // it's a message
            String AddresseeAndMessage            = temp.substring(doubleColonIndex + 2);
            String Addressee                      = AddresseeAndMessage.substring(0, AddresseeAndMessage.indexOf(":"));
            Addressee.trim();
            if (Addressee == stationCallsign) {     // it's a message for me!
                queryMessage = APRS_IS_Utils::processReceivedLoRaMessage(Sender, AddresseeAndMessage, thirdPartyPacket,
                                                                          stationCallsign);
            }
        }
        if (queryMessage) { TELEMETRY_Utils::incDrop(); return; }                  // answer should not be repeated.

        String loraPacket = generateDigipeatedPacket(packet, thirdPartyPacket);
        if (loraPacket != "") {
            const int informationIndex = temp.indexOf(":");
            const int destinationStart = temp.indexOf('>') + 1;
            const int comma = temp.indexOf(',', destinationStart);
            const String aprsDestination = temp.substring(destinationStart,
                comma != -1 && comma < informationIndex ? comma : informationIndex);
            if (informationIndex < 0 ||
                !STATION_Utils::claimPacketDestination(
                    Sender,
                    temp.substring(informationIndex + 1),
                    STATION_Utils::DEDUP_DIGI, aprsDestination)) {
                SD_Utils::setDecision("DUP");
                TELEMETRY_Utils::incDrop();
                Utils::println("[DE-DUPE] Digipeat skipped for: " + Sender);
                return;
            }
            SD_Utils::setDecision("RELAY");
            TELEMETRY_Utils::incRelay();
            // This is the ONE genuine case where RXT is legitimate: a frame
            // this station's own LoRa receiver just heard, being relayed
            // onward unmodified in content (only the path changes). Every
            // other addToOutputPacketBuffer() call site in the codebase
            // must leave eligibleForRxt at its default (false).
            STATION_Utils::addToOutputPacketBuffer(loraPacket, false, true);
            if (Config.digi.ecoMode != 1) displayToggle(true);
            lastScreenOn = millis();
        } else {
            SD_Utils::setDecision("PATH");
            TELEMETRY_Utils::incDrop();
        }
    }

}
