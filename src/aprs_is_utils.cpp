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
#include <WiFiClient.h>
#include "configuration.h"
#include "network_manager.h"
#include "aprs_is_utils.h"
#include "station_utils.h"
#include "board_pinout.h"
#include "syslog_utils.h"
#include "query_utils.h"
#include "A7670_utils.h"
#include "digi_utils.h"
#include "tnc_utils.h"
#include "lora_utils.h"
#include "aprs_return_route.h"
#include "aprs_message_number.h"
#include "local_message_gate.h"
#include "aprs_telemetry_rx.h"
#include "aprs_telemetry_persistence.h"
#include "display.h"
#include "utils.h"


extern Configuration        Config;
extern APRSPacket           lastAprsPacket;
extern NetworkManager       *networkManager;
extern WiFiClient           aprsIsClient;
extern uint32_t             lastScreenOn;
extern String               firstLine;
extern String               secondLine;
extern String               thirdLine;
extern String               fourthLine;
extern String               fifthLine;
extern String               sixthLine;
extern String               seventhLine;
extern bool                 modemLoggedToAPRSIS;
extern bool                 backupDigiMode;
extern String               versionNumber;
extern APRS_Telemetry_RX::Store aprsTelemetryStore;

bool        passcodeValid   = false;
uint32_t    lastServerCheck = 0;


#ifdef HAS_A7670
    extern bool             stationBeacon;
#endif


namespace APRS_IS_Utils {

    void upload(const String& line) {
        aprsIsClient.print(line + "\r\n");
    }

    void connect() {
        Serial.print("Connecting to APRS-IS ...     ");
        uint8_t count = 0;
        while (!aprsIsClient.connect(Config.aprs_is.server.c_str(), Config.aprs_is.port) && count < 20) {
            Serial.println("Didn't connect with server...");
            delay(1000);
            aprsIsClient.stop();
            aprsIsClient.flush();
            Serial.println("Run client.stop");
            Serial.println("Trying to connect with Server: " + String(Config.aprs_is.server) + " AprsServerPort: " + String(Config.aprs_is.port));
            count++;
            Serial.println("Try: " + String(count));
        }
        if (count == 20) {
            Serial.println("Tried: " + String(count) + " FAILED!");
        } else {
            Serial.println("Connected!\n(Server: " + String(Config.aprs_is.server) + " / Port: " + String(Config.aprs_is.port) + ")");
            String aprsAuth = "user ";
            aprsAuth += Config.callsign;
            aprsAuth += " pass ";
            aprsAuth += Config.aprs_is.passcode;
            aprsAuth += " vers CA2RXUiGate ";
            aprsAuth += versionNumber;
            aprsAuth += " filter ";
            aprsAuth += Config.aprs_is.filter;
            upload(aprsAuth);
        }
    }

    void updateWiFiAPRSISDisplayInfo() {
        static String lastWifiState     = "";
        static String lastAprsisState   = "";
        String wifiState, aprsisState;
        if (networkManager->isWiFiConnected()) {
            wifiState = "OK";
        } else {
            if (backupDigiMode || Config.digi.ecoMode == 1 || Config.digi.ecoMode == 2) {
                wifiState = "--";
            } else {
                wifiState = "AP";
            }
        }

        if (!Config.aprs_is.active) {
            aprsisState = "OFF";
        } else {
            #ifdef HAS_A7670
                if (modemLoggedToAPRSIS) {
                    aprsisState = "OK";
                } else {
                    aprsisState = "--";
                }
            #else
                if (aprsIsClient.connected()) {
                    aprsisState = "OK";
                } else {
                    aprsisState = "--";
                }
            #endif
        }

        if (wifiState != lastWifiState || aprsisState != lastAprsisState) {     // wake display only on status change (not every loop) so display timeout works
            if (!Config.display.alwaysOn && Config.display.timeout != 0) displayToggle(true);
            lastScreenOn    = millis();
            lastWifiState   = wifiState;
            lastAprsisState = aprsisState;
        }

        secondLine = "WiFi: ";
        secondLine += wifiState;
        secondLine += " APRS-IS: ";
        secondLine += aprsisState;
    }

    String buildPacketToUpload(const String& packet) {
        int colonIndex = packet.indexOf(":");
        String packetToUpload = packet.substring(0, colonIndex);
        if (Config.aprs_is.active && passcodeValid && Config.aprs_is.messagesToRF) {
            packetToUpload += ",qAR,";
        } else {
            packetToUpload += ",qAO,";
        }
        packetToUpload += Config.callsign;
        packetToUpload += APRSPacketLib::checkForStartingBytes(packet.substring(colonIndex));

        // RXT is an RF-only extension. Remove it at the APRS-IS boundary.
        return LoRa_Utils::stripRxtTrailer(packetToUpload);
    }

    static LocalMessageGate localMessages;

    void beginLoRaReception() {
        localMessages.beginReception();
    }

    bool processReceivedLoRaMessage(const String& sender, const String& packet, bool thirdParty,
                                    const String& responder) {
        int leftCurlyBraceIndex = packet.indexOf("{");
        int colonIndex          = packet.indexOf(":");
        const String receivedMessage = leftCurlyBraceIndex > 0
            ? packet.substring(colonIndex + 1, leftCurlyBraceIndex)
            : packet.substring(colonIndex + 1);
        const bool isQuery = receivedMessage.startsWith("?") || receivedMessage.startsWith("!RC1:");
        const auto decision = localMessages.claim(
            std::string(sender.c_str(), sender.length()),
            std::string(packet.c_str(), packet.length()), isQuery, millis());

        if (decision.acknowledge && leftCurlyBraceIndex > 0) {     // ack?
            String messageNumber = packet.substring(leftCurlyBraceIndex + 1);
            messageNumber.trim();

            const std::string ack = APRS_MESSAGE::ackText(
                std::string(messageNumber.c_str(), messageNumber.length()));

            if (sender.length() >= 1 && sender.length() <= 9 && !ack.empty()) {
                String addToBuffer = STATION_Utils::localReplyHeader(responder, sender, thirdParty);
                addToBuffer += "::";

                String processedSender = sender;
                while (processedSender.length() < 9) processedSender += ' ';
                addToBuffer += processedSender;
                addToBuffer += ":";
                addToBuffer += ack.c_str();
                STATION_Utils::addToOutputPacketBuffer(addToBuffer);
            } else {
                Serial.println("APRS message ACK suppressed: invalid sender or message number");
            }
        }
        if (isQuery) {
            // A duplicate query is still consumed locally; it must not fall
            // through into APRS-IS upload or RF relay handling.
            if (!decision.executeQuery) return true;
            if (!Config.display.alwaysOn && Config.display.timeout != 0) {
                displayToggle(true);
            }
            STATION_Utils::addToOutputPacketBuffer(QUERY_Utils::process(receivedMessage, sender, false, thirdParty, responder));
            lastScreenOn = millis();
            displayShow(firstLine, secondLine, thirdLine, fourthLine, fifthLine, "Callsign = " + sender, "TYPE --> QUERY", 0);
            return true;
        } else {
            return false;
        }
    }

    void processLoRaPacket(const String& packet) {
        if (passcodeValid && (aprsIsClient.connected() || modemLoggedToAPRSIS)) {
            if (packet.indexOf("NOGATE") == -1 && packet.indexOf("RFONLY") == -1) {
                int firstColonIndex = packet.indexOf(":");
                if (firstColonIndex > 5 && firstColonIndex < (packet.length() - 1) && packet[firstColonIndex + 1] != '}' && packet.indexOf("TCPIP") == -1) {
                    const String& Sender = packet.substring(0, packet.indexOf(">"));
                    if (Sender != Config.callsign && Utils::callsignIsValid(Sender)) {
                        Utils::updateLoRaPacketDisplayInfo(lastAprsPacket, 0);  // LoRa-APRS
                        int doubleColonIndex = packet.indexOf("::");
                        const String& AddresseeAndMessage = packet.substring(doubleColonIndex + 2);
                        String Addressee = AddresseeAndMessage.substring(0, AddresseeAndMessage.indexOf(":"));
                        Addressee.trim();
                        bool queryMessage = false;
                        if (doubleColonIndex > 10 && Addressee == Config.callsign) {      // its a message for me!
                            queryMessage = processReceivedLoRaMessage(Sender, APRSPacketLib::checkForStartingBytes(AddresseeAndMessage), false,
                                                                  Config.callsign);
                        }
                        if (queryMessage) return;

                        const String& aprsPacketToUpload = buildPacketToUpload(packet);
                        if (!Config.display.alwaysOn && Config.display.timeout != 0) {
                            displayToggle(true);
                        }
                        lastScreenOn = millis();
                        #ifdef HAS_A7670
                            stationBeacon = true;
                            A7670_Utils::uploadToAPRSIS(aprsPacketToUpload);
                            stationBeacon = false;
                        #else
                            upload(aprsPacketToUpload);
                        #endif
                        Utils::println("(Uploaded to APRS-IS)");
                        displayShow(firstLine, secondLine, thirdLine, fourthLine, fifthLine, sixthLine, seventhLine, 0);
                    }
                }
            }
        }
    }

    String buildPacketToTx(const String& aprsisPacket, uint8_t packetType) {
        String packet = aprsisPacket;
        packet.trim();
        String outputPacket = APRSPacketLib::generateBasePacket(Config.callsign, "APLRG1", Config.beacon.path);
        outputPacket += ":}";
        outputPacket += packet.substring(0, packet.indexOf(",")); // Callsign>Tocall
        outputPacket.concat(",TCPIP,");
        outputPacket.concat(Config.callsign);
        outputPacket.concat("*");
        int colonEqualIndex         = packet.indexOf(":=");
        int doubleColonIndex        = packet.indexOf("::");
        int colonInvAccentIndex     = packet.indexOf(":`");

        switch (packetType) {
            case 0: // gps
                if (colonEqualIndex > 0) {
                    outputPacket += packet.substring(colonEqualIndex);
                } else {
                    outputPacket += packet.substring(packet.indexOf(":!"));
                }
                break;
            case 1: // messages
                outputPacket += packet.substring(doubleColonIndex);
                break;
            case 2: // status
                outputPacket += packet.substring(packet.indexOf(":>"));
                break;
            case 3: // telemetry
                outputPacket += packet.substring(doubleColonIndex);
                break;
            case 4: // mic-e
                if (colonInvAccentIndex > 0) {
                    outputPacket += packet.substring(colonInvAccentIndex);
                } else {
                    outputPacket += packet.substring(packet.indexOf(":'"));
                }
                break;
            case 5: // object
                outputPacket += packet.substring(packet.indexOf(":;"));
                break;
        }
        return outputPacket;
    }

    String buildPacketToTx(const String& aprsisPacket, uint8_t packetType,
                           const String& path) {
        if (packetType != 1) return "";
        String packet = aprsisPacket;
        packet.trim();
        const std::string output = APRS_RETURN_ROUTE::buildThirdPartyMessage(
            std::string(Config.callsign.c_str(), Config.callsign.length()),
            std::string(path.c_str(), path.length()),
            std::string(packet.c_str(), packet.length()));
        return String(output.c_str());
    }

    void processAckMessage(const String& sender, const String& message) {
        String ackPacket = Config.callsign;
        ackPacket += ">APLRG1,TCPIP,qAC::";

        String senderCallsign = sender;
        for (int i = sender.length(); i < 9; i++) {
            senderCallsign += ' ';
        }
        ackPacket += senderCallsign;
        ackPacket += ":";

        String ackMessage = "ack";
        ackMessage += message.substring(message.indexOf("{") + 1);
        ackMessage.trim();
        ackPacket += ackMessage;

        #ifdef HAS_A7670
            A7670_Utils::uploadToAPRSIS(ackPacket);
        #else
            upload(ackPacket);
        #endif
    }

    void processAPRSISPacket(const String& packet) {
        uint32_t currentTime = millis();
        if (!passcodeValid && packet.indexOf(Config.callsign) != -1) {
            if (packet.indexOf("unverified") != -1 ) {
                Serial.println("\n****APRS PASSCODE NOT VALID****\n");
                displayShow(firstLine, "", "    APRS PASSCODE", "    NOT VALID !!!", "", "", "", 3000);
                aprsIsClient.stop();
                Config.aprs_is.active = false;
            } else if (packet.indexOf("verified") != -1 ) {
                if (Config.digi.backupDigiMode) lastServerCheck = currentTime;
                passcodeValid = true;
            }
        }
        if (passcodeValid) {
            if (packet.startsWith("#")) {
                if (Config.digi.backupDigiMode) lastServerCheck = currentTime;
            } else {
                std::string telemetryStation;
                std::string telemetryKind;
                if (APRS_Telemetry_RX::metadataDescriptor(packet.c_str(), telemetryStation, telemetryKind)) {
                    aprsTelemetryStore.ingest(packet.c_str(), "", currentTime);
                    APRS_Telemetry_Persistence::remember(packet);
                }
                int doubleColonIndex = packet.indexOf("::");
                if (Config.aprs_is.messagesToRF && doubleColonIndex > 0) {
                    String Sender = packet.substring(0, packet.indexOf(">"));
                    const String& AddresseeAndMessage = packet.substring(doubleColonIndex + 2);
                    int colonIndex = AddresseeAndMessage.indexOf(":");
                    String Addressee = AddresseeAndMessage.substring(0, colonIndex);
                    Addressee.trim();
                    if (Addressee == Config.callsign) {                 // its for me!
                        String receivedMessage;
                        int curlyBraceIndex = AddresseeAndMessage.indexOf("{");
                        if (curlyBraceIndex > 0) {     // ack?
                            processAckMessage(Sender, AddresseeAndMessage);
                            receivedMessage = AddresseeAndMessage.substring(colonIndex + 1, curlyBraceIndex);
                        } else {
                            receivedMessage = AddresseeAndMessage.substring(colonIndex + 1);
                        }
                        if (receivedMessage.indexOf("?") == 0 || receivedMessage.startsWith("!RC1:")) {
                            Utils::println("Rx Query (APRS-IS)  : " + packet);
                            String queryAnswer = QUERY_Utils::process(receivedMessage, Sender, true, false, Config.callsign);
                            if (!Config.display.alwaysOn && Config.display.timeout != 0) {
                                displayToggle(true);
                            }
                            lastScreenOn = currentTime;
                            #ifdef HAS_A7670
                                A7670_Utils::uploadToAPRSIS(queryAnswer);
                            #else
                                upload(queryAnswer);
                            #endif
                            SYSLOG_Utils::logAPRSISTx(queryAnswer);
                            fifthLine = "APRS-IS ----> APRS-IS";
                            sixthLine = Config.callsign;
                            for (int j = sixthLine.length();j < 9;j++) {
                                sixthLine += " ";
                            }
                            sixthLine += "> ";
                            sixthLine += Sender;
                            seventhLine = "QUERY = ";
                            seventhLine += receivedMessage;
                        }
                        displayShow(firstLine, secondLine, thirdLine, fourthLine, fifthLine, sixthLine, seventhLine, 0);
                    } else {
                        Utils::print("Rx Message (APRS-IS): " + packet);
                        const bool isTelemetryMetadata = packet.indexOf("EQNS.") != -1 ||
                                                         packet.indexOf("UNIT.") != -1 ||
                                                         packet.indexOf("PARM.") != -1 ||
                                                         packet.indexOf("BITS.") != -1;
                        String returnPath;
                        if (!isTelemetryMetadata && STATION_Utils::getReturnPath(Addressee, returnPath)) {
                            Utils::println("[RETURN-PATH] Message to " + Addressee + " via " +
                                           (returnPath == "" ? String("DIRECT") : returnPath));
                            const String rfPacket = buildPacketToTx(packet, 1, returnPath);
                            if (rfPacket != "") {
                                STATION_Utils::addToOutputPacketBuffer(rfPacket);
                                displayToggle(true);
                                lastScreenOn = currentTime;
                                Utils::updateAPRSISPacketDisplayInfo(packet); // APRS-LoRa
                                displayShow(firstLine, secondLine, thirdLine, fourthLine, fifthLine, sixthLine, seventhLine, 0);
                            }
                        } else if (!isTelemetryMetadata) {
                            Utils::println("[RETURN-PATH] No learned route for " + Addressee + ": No Tx");
                        }
                    }
                } else if (Config.aprs_is.objectsToRF && packet.indexOf(":;") > 0) {
                    Utils::print("Rx Object (APRS-IS) : " + packet);
                    if (STATION_Utils::checkObjectTime(packet)) {
                        STATION_Utils::addToOutputPacketBuffer(buildPacketToTx(packet, 5));
                        displayToggle(true);
                        lastScreenOn = currentTime;
                        Utils::updateAPRSISPacketDisplayInfo(packet); // APRS-LoRa
                        Serial.println();
                    } else {
                        Serial.println(" ---> Rejected (Time): No Tx");
                    }
                }
                if (Config.tnc.aprsBridgeActive) {
                    // Packets bridged from APRS-IS never carry RXT telemetry --
                    // RXT is strictly an RF-local feature (enforced upstream by
                    // LoRa_Utils::receivePacket(), which strips any trailer
                    // before a packet is handed anywhere). Pass an empty
                    // hop-metrics vector rather than calling
                    // getDecodedRxtMetrics() here, which would incorrectly
                    // reuse whatever RXT state is left over from the last
                    // real RF receive and attach it to an unrelated packet.
                    static const std::vector<LoRa_Utils::RxtHopMetric> noHopMetrics;
                    if (Config.tnc.enableServer) TNC_Utils::sendToClients(packet, false, noHopMetrics);  // Send received packet to TNC KISS
                    if (Config.tnc.enableSerial) TNC_Utils::sendToSerial(packet, false, noHopMetrics);   // Send received packet to Serial KISS
                }
            }
        }
    }

    void listenAPRSIS() {
        #ifdef HAS_A7670
            A7670_Utils::listenAPRSIS();
        #else
            if (aprsIsClient.connected()) {
                if (aprsIsClient.available()) {
                    String aprsisPacket = aprsIsClient.readStringUntil('\r');
                    aprsisPacket.trim();
                    processAPRSISPacket(aprsisPacket);
                }
            }
        #endif
    }

    void firstConnection() {
        if (Config.aprs_is.active && networkManager->isConnected() && !aprsIsClient.connected()) {
            connect();
            while (!passcodeValid) {
                listenAPRSIS();
            }
        }
    }

}
