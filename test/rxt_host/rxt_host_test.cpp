#include "rxt_protocol.h"

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

}

int main() {
    const std::string packet = "SRC>DST,F6DEV*,WIDE2-2*,F4MLV-10*,WIDE2-1:payload";
    expectNodes("multiple stars", RXT_Protocol::usedPathNodes(packet),
                {"F6DEV", "WIDE2-2", "F4MLV-10"});
    expectNodes("single last-used star",
                RXT_Protocol::usedPathNodes("SRC>DST,F6DEV,F4MLV-10*,WIDE2-1:payload"),
                {"F6DEV", "F4MLV-10"});
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

    if (failures != 0) return 1;
    std::cout << "RXT host tests: 19 passed, 0 failed\n";
    return 0;
}
