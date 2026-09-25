#include "rxt_protocol.h"
#include "aprs_telemetry_rx.h"
#include "aprs_json_text.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expectEqual(const std::string& name, const std::string& actual, const std::string& expected) {
    if (actual == expected) return;
    std::cerr << "FAIL " << name << ": expected [" << expected << "], got [" << actual << "]\n";
    failures++;
}

void expectNodes(const std::string& name, const std::vector<std::string>& actual,
                 const std::vector<std::string>& expected) {
    if (actual == expected) return;
    std::cerr << "FAIL " << name << '\n';
    failures++;
}

void expectBool(const std::string& name, bool actual, bool expected) {
    if (actual == expected) return;
    std::cerr << "FAIL " << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void expectNear(const std::string& name, float actual, float expected) {
    if (actual > expected - 0.0001F && actual < expected + 0.0001F) return;
    std::cerr << "FAIL " << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

}

int main() {
    const uint8_t ordinaryText[] = {'A', 'P', 'R', 'S', ' ', '"', '\\'};
    expectBool("safe ASCII JSON text",
               APRS_JSON_Text::isSafeUtf8(ordinaryText, sizeof(ordinaryText)), true);
    const uint8_t utf8Text[] = {0x46, 0x34, 0x4d, 0x4c, 0x56, 0x20, 0xc3, 0xa9};
    expectBool("safe UTF-8 JSON text",
               APRS_JSON_Text::isSafeUtf8(utf8Text, sizeof(utf8Text)), true);
    const uint8_t micEDti[] = {0x1c, 'w', '2', '5'};
    expectBool("Mic-E control DTI omitted from text",
               APRS_JSON_Text::isSafeUtf8(micEDti, sizeof(micEDti)), false);
    const uint8_t oldMicEDti[] = {0x1d, 'w', '2', '5'};
    expectBool("old Mic-E control DTI omitted from text",
               APRS_JSON_Text::isSafeUtf8(oldMicEDti, sizeof(oldMicEDti)), false);
    const uint8_t newline[] = {'A', '\n', 'B'};
    expectBool("newline omitted from text",
               APRS_JSON_Text::isSafeUtf8(newline, sizeof(newline)), false);
    const uint8_t del[] = {'A', 0x7f, 'B'};
    expectBool("DEL omitted from text", APRS_JSON_Text::isSafeUtf8(del, sizeof(del)), false);
    const uint8_t malformedUtf8[] = {0xc3, 0x28};
    expectBool("malformed UTF-8 omitted from text",
               APRS_JSON_Text::isSafeUtf8(malformedUtf8, sizeof(malformedUtf8)), false);

    const std::string packet = "SRC>DST,F6DEV*,WIDE2-2*,F4MLV-10*,WIDE2-1:payload";
    expectNodes("multiple stars", RXT_Protocol::usedPathNodes(packet),
                {"F6DEV", "F4MLV-10"});
    expectNodes("starred aliases are not physical hops",
                RXT_Protocol::usedPathNodes(
                    "F6ZZX-1>APLRG1,F6DEV*,WIDE2-2*,F4MLV-10*:payload"),
                {"F6DEV", "F4MLV-10"});
    expectNodes("single last-used star",
                RXT_Protocol::usedPathNodes("SRC>DST,F6DEV,F4MLV-10*,WIDE2-1:payload"),
                {"F6DEV", "F4MLV-10"});
    expectNodes("Jon three-hop mixed stars",
                RXT_Protocol::usedPathNodes(
                    "SUNSET>APLRG1,SOMTNX,TSRXBX*,TSRXAX*:payload"),
                {"SOMTNX", "TSRXBX", "TSRXAX"});
    expectNodes("unused path", RXT_Protocol::usedPathNodes("SRC>DST,WIDE1-1,WIDE2-1:payload"), {});
    expectNodes("payload comma is not a path",
                RXT_Protocol::usedPathNodes("SRC>DST:payload,with,commas"), {});

    expectEqual("append first tuple", RXT_Protocol::attachTrailer("SRC>DST:payload", "ABCD"),
                "SRC>DST:payload{ABCD}");
    expectEqual("append second tuple", RXT_Protocol::attachTrailer("SRC>DST:payload{ABCD}", "EFGH"),
                "SRC>DST:payload{ABCDEFGH}");
    expectEqual("three tuple cap", RXT_Protocol::attachTrailer("SRC>DST:payload{ABCDEFGHIJKL}", "MNOP"),
                "SRC>DST:payload{ABCDEFGHIJKL}");
    expectEqual("preserve ordinary brace suffix", RXT_Protocol::attachTrailer("SRC>DST:payload{hello}", "ABCD"),
                "SRC>DST:payload{hello}{ABCD}");

    std::string tuples;
    expectEqual("strip trailer", RXT_Protocol::stripTrailer("SRC>DST:payload{ABCDEFGH}", &tuples),
                "SRC>DST:payload");
    expectEqual("decoded tuples", tuples, "ABCDEFGH");
    expectEqual("keep ordinary brace suffix", RXT_Protocol::stripTrailer("SRC>DST:payload{hello}", &tuples),
                "SRC>DST:payload{hello}");
    expectEqual("no tuples from ordinary suffix", tuples, "");

    expectBool("direct APRS message",
               RXT_Protocol::isAprsMessage("SRC>DST::TARGET   :hello{1"), true);
    expectBool("direct ACK",
               RXT_Protocol::isAprsMessage("SRC>DST::TARGET   :ack1"), true);
    expectBool("third-party APRS message",
               RXT_Protocol::isAprsMessage("IGATE>DST:}SRC>DST::TARGET   :hello{1"), true);
    expectBool("nested third-party APRS message",
               RXT_Protocol::isAprsMessage("OUTER>DST:}IGATE>DST:}SRC>DST::TARGET   :rej1"), true);
    expectBool("ordinary position",
               RXT_Protocol::isAprsMessage("SRC>DST:!4903.50N/07201.75W-Test"), false);
    expectBool("third-party position",
               RXT_Protocol::isAprsMessage("IGATE>DST:}SRC>DST:!4903.50N/07201.75W-Test"), false);
    expectBool("malformed packet", RXT_Protocol::isAprsMessage("not a packet"), false);

    APRS_Telemetry_RX::Store telemetry;
    expectBool("PARM metadata", telemetry.ingest(
        "SRC>APRS::F4MLV-2  :PARM.V_Batt,V_Ext,RX_rate,RelRate,DrpRate", "10:00:00", 1), true);
    expectBool("UNIT metadata", telemetry.ingest(
        "SRC>APRS::F4MLV-2  :UNIT.VDC,VDC,pkt/h,pkt/h,pkt/h", "10:00:01", 2), true);
    expectBool("EQNS metadata", telemetry.ingest(
        "SRC>APRS::F4MLV-2  :EQNS.0,.01,0,0,.02,0,0,1,0,0,1,0,0,1,0", "10:00:02", 3), true);
    expectBool("base91 telemetry", telemetry.ingest(
        "F4MLV-2>APLRG1:=L8gjuNmQ`a test|!A%e!Q!=!5|", "10:00:03", 4), true);
    expectBool("one telemetry station", telemetry.stations().size() == 1, true);
    const APRS_Telemetry_RX::Station& base91 = telemetry.stations()[0];
    expectEqual("telemetry identity", base91.callsign, "F4MLV-2");
    expectEqual("telemetry format", base91.format, "Base91");
    expectBool("four base91 channels", base91.analogCount == 4, true);
    expectNear("calibrated first channel", APRS_Telemetry_RX::calibratedValue(base91, 0),
               base91.analog[0] * 0.01F);

    expectBool("traditional telemetry", telemetry.ingest(
        "N0QBF-11>APRS:T#005,199,000,255,073,123,01101001", "10:00:04", 5), true);
    const APRS_Telemetry_RX::Station& traditional = telemetry.stations()[1];
    expectEqual("traditional sequence", traditional.sequence, "005");
    expectNear("traditional A1", traditional.analog[0], 199.0F);
    expectBool("traditional B2", (traditional.digital & 0x02U) != 0, true);

    expectBool("bit metadata", telemetry.ingest(
        "SRC>APRS::N0QBF-11 :BITS.10110000,Big Balloon", "10:00:05", 6), true);
    expectEqual("project title", telemetry.stations()[1].project.data(), "Big Balloon");
    expectBool("bit sense B1", telemetry.stations()[1].bitSense[0], true);
    expectBool("bit sense B2", telemetry.stations()[1].bitSense[1], false);

    expectBool("third party telemetry", telemetry.ingest(
        "IGATE>APRS:}INNER-GS>APRS:T#123,1,2,3,4,5,10000000", "10:00:06", 7), true);
    expectEqual("third party identity", telemetry.stations()[2].callsign, "INNER-GS");

    std::string metadataStation;
    std::string metadataKind;
    expectBool("metadata descriptor", APRS_Telemetry_RX::metadataDescriptor(
        "SRC>APRS::F4MLV-2  :EQNS.0,.01,0", metadataStation, metadataKind), true);
    expectEqual("metadata target", metadataStation, "F4MLV-2");
    expectEqual("metadata kind", metadataKind, "EQNS");

    if (failures != 0) return 1;
    std::cout << "RXT and APRS telemetry host tests passed\n";
    return 0;
}
