#include <Arduino.h>
#include <APRSPacketLib.h>
#include <configuration.h>
#include <digi_utils.h>

#include <cstdint>
#include <cstdio>

namespace DIGI_Utils {
// Internal builder exposed only to this harness to test its cross-frequency guard.
String buildPacket(const String& path, const String& packet, bool thirdParty, bool crossFreq);
}

Configuration Config;
bool backupDigiMode = false;
uint32_t lastScreenOn = 0;
String iGateBeaconPacket;
String firstLine;
String secondLine;
String thirdLine;
String fourthLine;
String fifthLine;
String sixthLine;
String seventhLine;

namespace STATION_Utils {
bool isIn25SegHashBuffer(const String&, const String&) { return false; }
void updateLastHeard(const String&) {}
bool claimPacketDestination(const String&, const String&, uint8_t, const String&) { return true; }
void addToOutputPacketBuffer(const String&, bool, bool) {}
}

APRSPacket lastAprsPacket;

namespace APRSPacketLib {
String checkForStartingBytes(const String& packet) { return packet; }
}

namespace APRS_IS_Utils {
String lastResponder;
bool processReceivedLoRaMessage(const String&, const String&, bool, const String& responder) {
    lastResponder = responder;
    return true;
}
}

namespace Utils {
bool callsignIsValid(const String&) { return true; }
void updateLoRaPacketDisplayInfo(APRSPacket&, uint8_t) {}
void println(const String&) {}
}

namespace SD_Utils {
void setDecision(const String&) {}
}

namespace TELEMETRY_Utils {
void incDrop() {}
void incRelay() {}
}

void displayToggle(bool) {}

namespace {
int passed = 0;
int failed = 0;
constexpr const char* NONE = "<not digipeated>";

struct Options {
    int txFreq = 433775000;
    int rxFreq = 433775000;
    const char* regionalAliases = "WIDE";
    int regionalMaxHops = 2;
    bool thirdParty = false;
    const char* callsign = "F4MLV-10";
};

void check(const char* label, int mode, const char* packet,
           const char* expected, Options options = Options()) {
    Config.callsign = options.callsign;
    Config.tacticalCallsign = "";
    Config.digi.mode = mode;
    Config.digi.regionalAliases = options.regionalAliases;
    Config.digi.regionalMaxHops = options.regionalMaxHops;
    Config.loramodule.txFreq = options.txFreq;
    Config.loramodule.rxFreq = options.rxFreq;

    String output = DIGI_Utils::generateDigipeatedPacket(
        String(packet), options.thirdParty);
    String actual = output.length() ? output : String(NONE);
    bool ok = actual == String(expected);

    std::printf("%s  mode %d  %s\n", ok ? "PASS" : "FAIL", mode, label);
    if (ok) {
        ++passed;
        return;
    }

    std::printf("        in       : %s\n"
                "        expected : %s\n"
                "        got      : %s\n",
                packet, expected, actual.c_str());
    ++failed;
}
}

void checkResponder(const char* label, const char* tactical, const char* packet,
                    const char* expected) {
    Config.callsign = "F4MLV-15";
    Config.tacticalCallsign = tactical;
    Config.digi.mode = 2;
    APRS_IS_Utils::lastResponder = "";
    DIGI_Utils::processLoRaPacket(String(packet));
    bool ok = APRS_IS_Utils::lastResponder == String(expected);
    std::printf("%s  responder  %s\n", ok ? "PASS" : "FAIL", label);
    if (ok) { ++passed; return; }
    std::printf("        expected : %s\n        got      : %s\n",
                expected, APRS_IS_Utils::lastResponder.c_str());
    ++failed;
}

int main() {
    checkResponder("message to tactical answered as tactical", "F4MLV-1",
                   "F4MLV-9>APLRG1::F4MLV-1  :?APRSV{I2", "F4MLV-1");
    checkResponder("message to callsign without tactical answered as callsign", "",
                   "F4MLV-9>APLRG1::F4MLV-15 :?APRSV{I1", "F4MLV-15");
    for (int mode : {1, 2, 3}) {
        check("lowercase explicit callsign", mode,
              "F4MLV-7>APLRT1,f4mlv-10,WIDE1-1:>test",
              "F4MLV-7>APLRT1,f4mlv-10*,WIDE1-1:>test");
        check("mixed-case callsign after previous hop", mode,
              "F4MLV-7>APLRT1,RELAY1*,f4MLv-10:>test",
              "F4MLV-7>APLRT1,RELAY1,f4MLv-10*:>test");
        check("lowercase already starred refuses loop", mode,
              "F4MLV-7>APLRT1,f4mlv-10*,WIDE1-1:>test", NONE);
        check("lowercase implicitly used refuses loop", mode,
              "F4MLV-7>APLRT1,f4mlv-10,RELAY1*,WIDE1-1:>test", NONE);
        check("duplicate identity with different case refused", mode,
              "F4MLV-7>APLRT1,F4MLV-10,f4mlv-10,WIDE1-1:>test", NONE);
        check("lowercase own hop cannot bypass previous unused relay", mode,
              "F4MLV-7>APLRT1,RELAY1,f4mlv-10,WIDE1-1:>test", NONE);
    }
    check("local reply first explicit hop before RFONLY", 2,
          "F4MLV-15>APLRG1,F4MLV-10,F4MLV-2,RFONLY::F4MLV-7  :ack01",
          "F4MLV-15>APLRG1,F4MLV-10*,F4MLV-2,RFONLY::F4MLV-7  :ack01");
    check("local reply second explicit hop before RFONLY", 2,
          "F4MLV-15>APLRG1,F4MLV-2*,F4MLV-10,RFONLY::F4MLV-7  :ack01",
          "F4MLV-15>APLRG1,F4MLV-2,F4MLV-10*,RFONLY::F4MLV-7  :ack01");
    std::printf("digi_utils.cpp - digipeated path output\n\n");

    std::printf("-- mode 1 : WIDE1-1 fill-in --\n");
    check("WIDE1-1 consumed", 1,
          "F4MLV-7>APLRT1,WIDE1-1:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F4MLV-10*:=/8gk=NmQF[LWQ");
    check("WIDE1-1 already marked -> refused", 1,
          "F4MLV-7>APLRT1,F6DEV-10*,WIDE1-1:=/8gk=NmQF[LWQ", NONE);
    check("WIDE1-1 after an unused hop -> refused", 1,
          "F4MLV-7>APLRT1,F6DEV-10,WIDE1-1:=/8gk=NmQF[LWQ", NONE);
    check("WIDE2 only -> not this mode", 1,
          "F4MLV-7>APLRT1,WIDE2-1:=/8gk=NmQF[LWQ", NONE);
    check("no path at all", 1,
          "F4MLV-7>APLRT1:=/8gk=NmQF[LWQ", NONE);
    check("explicit own callsign", 1,
          "F4MLV-7>APLRT1,F4MLV-10:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F4MLV-10*:=/8gk=NmQF[LWQ");
    check("explicit own callsign after previous hop", 1,
          "F4MLV-7>APLRT1,SOMTNP*,F4MLV-10:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,SOMTNP,F4MLV-10*:=/8gk=NmQF[LWQ");
    check("explicit own callsign already consumed", 1,
          "F4MLV-7>APLRT1,F4MLV-10*:=/8gk=NmQF[LWQ", NONE);

    std::printf("\n-- mode 2 : WIDE1-1 + WIDE2-n --\n");
    check("WIDE1-1 first", 2,
          "F4MLV-7>APLRT1,WIDE1-1,WIDE2-1:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F4MLV-10*,WIDE2-1:=/8gk=NmQF[LWQ");
    check("WIDE2-1 alone", 2,
          "F4MLV-7>APLRT1,WIDE2-1:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F4MLV-10*:=/8gk=NmQF[LWQ");
    check("WIDE2-2 decremented", 2,
          "F4MLV-7>APLRT1,WIDE2-2:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F4MLV-10*,WIDE2-1:=/8gk=NmQF[LWQ");
    check("WIDE2 before WIDE1 -> refused", 2,
          "F4MLV-7>APLRT1,WIDE2-1,WIDE1-1:=/8gk=NmQF[LWQ", NONE);
    check("no WIDE left", 2,
          "F4MLV-7>APLRT1,F6DEV-10*:=/8gk=NmQF[LWQ", NONE);
    check("WIDE1 substring is not an alias", 2,
          "F4MLV-7>APLRT1,XWIDE1-1:=/8gk=NmQF[LWQ", NONE);
    check("explicit own callsign in regional mode", 2,
          "F4MLV-7>APLRT1,F4MLV-10,N7UV-6:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F4MLV-10*,N7UV-6:=/8gk=NmQF[LWQ");
    check("explicit own callsign after previous regional digi", 2,
          "F4MLV-7>APLRT1,SOMTNP*,F4MLV-10:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,SOMTNP,F4MLV-10*:=/8gk=NmQF[LWQ");

    std::printf("\n-- mode 2 : configurable regional aliases --\n");
    Options ariege;
    ariege.regionalAliases = "WIDE ARIEG";
    check("ARIEG1-1 consumed", 2,
          "F4MLV-7>APLRT1,ARIEG1-1:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F4MLV-10*:=/8gk=NmQF[LWQ", ariege);
    check("ARIEG2-2 decremented", 2,
          "F4MLV-7>APLRT1,ARIEG2-2:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F4MLV-10*,ARIEG2-1:=/8gk=NmQF[LWQ", ariege);
    check("ARIEG2-1 consumed", 2,
          "F4MLV-7>APLRT1,F6DEV-10*,ARIEG2-1:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F6DEV-10,F4MLV-10*:=/8gk=NmQF[LWQ", ariege);
    check("own starred callsign before regional alias refused", 2,
          "F4MLV-7>APLRT1,F4MLV-10*,ARIEG2-1:=/8gk=NmQF[LWQ",
          NONE, ariege);
    check("own canonical earlier callsign before regional alias refused", 2,
          "F4MLV-7>APLRT1,F4MLV-10,F6DEV-10*,ARIEG2-1:=/8gk=NmQF[LWQ",
          NONE, ariege);
    check("similar callsign does not trigger loop guard", 2,
          "F4MLV-7>APLRT1,F4MLV-1*,ARIEG2-1:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F4MLV-1,F4MLV-10*:=/8gk=NmQF[LWQ", ariege);
    check("fill-in before regional alias", 2,
          "F4MLV-7>APLRT1,WIDE1-1,ARIEG1-1:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F4MLV-10*,ARIEG1-1:=/8gk=NmQF[LWQ", ariege);
    check("regional alias before fill-in refused", 2,
          "F4MLV-7>APLRT1,ARIEG1-1,WIDE1-1:=/8gk=NmQF[LWQ", NONE, ariege);
    check("unconfigured alias refused", 2,
          "F4MLV-7>APLRT1,ARIEG1-1:=/8gk=NmQF[LWQ", NONE);
    check("hop count above configured maximum refused", 2,
          "F4MLV-7>APLRT1,ARIEG3-3:=/8gk=NmQF[LWQ", NONE, ariege);
    check("remaining count above initial count refused", 2,
          "F4MLV-7>APLRT1,ARIEG1-2:=/8gk=NmQF[LWQ", NONE, ariege);
    check("consumed regional alias refused", 2,
          "F4MLV-7>APLRT1,ARIEG2-1*:=/8gk=NmQF[LWQ", NONE, ariege);
    Options threeHops = ariege;
    threeHops.regionalMaxHops = 3;
    check("configured WIDEn-N above two hops", 2,
          "F4MLV-7>APLRT1,WIDE3-3:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F4MLV-10*,WIDE3-2:=/8gk=NmQF[LWQ", threeHops);
    check("configured regional n-N above two hops", 2,
          "F4MLV-7>APLRT1,ARIEG3-3:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F4MLV-10*,ARIEG3-2:=/8gk=NmQF[LWQ", threeHops);

    std::printf("\n-- mode 2 : exhausted hops (issue #443) --\n");
    check("WIDE2-1 already used", 2,
          "F4MLV-7>APLRT1,F6DEV-10*,WIDE2-1*:=/8gk=NmQF[LWQ", NONE);
    check("reported frame: earlier '*' + used WIDE2-1", 2,
          "F1ZCJ-3>APFD11,F6DEV*,F6DEV-11,WIDE2-1*,WIDE3-3:!4321.45N/00225.50E#",
          NONE);
    check("two consumed hops, WIDE2-1 free", 2,
          "F4MLV-7>APLRT1,F6DEV*,F6DEV-11*,WIDE2-1:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F6DEV,F6DEV-11,F4MLV-10*:=/8gk=NmQF[LWQ");

    std::printf("\n-- mode 3 : own callsign in path --\n");
    check("own callsign present", 3,
          "F4MLV-7>APLRT1,F4MLV-10,WIDE2-1:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F4MLV-10*,WIDE2-1:=/8gk=NmQF[LWQ");
    check("own callsign already marked", 3,
          "F4MLV-7>APLRT1,F4MLV-10*,WIDE2-1:=/8gk=NmQF[LWQ", NONE);
    check("own callsign absent", 3,
          "F4MLV-7>APLRT1,WIDE1-1,WIDE2-1:=/8gk=NmQF[LWQ", NONE);

    std::printf("\n-- cross-frequency digi (tx/rx >= 125 kHz apart) --\n");
    Options crossFrequency;
    crossFrequency.txFreq = 433900000;
    crossFrequency.rxFreq = 433775000;
    check("no WIDE, cross-freq", 1,
          "F4MLV-7>APLRT1,F6DEV-10*:=/8gk=NmQF[LWQ",
          "F4MLV-7>APLRT1,F6DEV-10,F4MLV-10*:=/8gk=NmQF[LWQ",
          crossFrequency);

    crossFrequency.callsign = "F4MLV-1";
    check("cross-freq similar callsign is not own identity", 1,
          "SRC>APLRT1,F4MLV-10*:>test",
          "SRC>APLRT1,F4MLV-10,F4MLV-1*:>test", crossFrequency);
    check("cross-freq similar source is not a path element", 1,
          "F4MLV-10>APLRT1:>test",
          "F4MLV-10>APLRT1,F4MLV-1*:>test", crossFrequency);
    check("cross-freq lowercase used identity refused", 1,
          "SRC>APLRT1,f4mlv-1*:>test", NONE, crossFrequency);

    // Exercise the cross-frequency guard directly: the public entry point
    // normally routes explicit own hops through processMode3Path instead.
    for (const char* path : {"F4MLV-1*", "f4mlv-1*", "RELAY*,f4MLv-1"}) {
        const String packet = String("SRC>APLRT1,") + path + ":>test";
        const bool ok = DIGI_Utils::buildPacket(path, packet, false, true).length() == 0;
        std::printf("%s  cross-freq own identity guard: %s\n", ok ? "PASS" : "FAIL", path);
        if (ok) ++passed; else ++failed;
    }

    std::printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
