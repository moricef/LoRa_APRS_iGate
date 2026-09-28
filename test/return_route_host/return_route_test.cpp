#include "aprs_return_route.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;
const std::vector<std::string> aliases{"WIDE", "ARIEG"};

void check(const char* label, const char* packet, bool valid, const char* expected) {
    const APRS_RETURN_ROUTE::Result result =
        APRS_RETURN_ROUTE::derive(packet, "N7UV-44", "TSRXBX", aliases);
    const bool ok = result.valid == valid && result.path == expected;
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) {
        std::printf("        valid/path expected: %d / %s\n"
                    "        valid/path got     : %d / %s\n",
                    valid, expected, result.valid, result.path.c_str());
        ++failures;
    }
}

} // namespace

int main() {
    const auto reply = [](const char* label, const char* received,
                          const char* expected) {
        const auto route = APRS_RETURN_ROUTE::derive(received, "N7UV-44", "TSRXBX", aliases);
        const auto header = APRS_RETURN_ROUTE::buildLocalReplyHeader(
            "N7UV-44", route, "WIDE1-1", true);
        const bool ok = header == expected;
        std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
        if (!ok) ++failures;
    };
    reply("local direct reply ignores beacon path",
          "N7UV-4>APLRT1,WIDE1-1::N7UV-44  :?APRSV{01",
          "N7UV-44>APLRG1,RFONLY");
    reply("local one-hop ACK/reply header",
          "N7UV-4>APLRT1,N7UV-6*,WIDE2-1::N7UV-44  :?APRSV{01",
          "N7UV-44>APLRG1,N7UV-6,RFONLY");
    reply("local two-hop ACK/reply header",
          "N7UV-4>APLRT1,N7UV-6,SOMTNP*,WIDE2-1::N7UV-44  :?APRSV{01",
          "N7UV-44>APLRG1,SOMTNP,N7UV-6,RFONLY");
    reply("RFONLY request still yields a return route",
          "N7UV-4>APLRT1,RFONLY,N7UV-6*,WIDE2-1::N7UV-44  :?APRSV{01",
          "N7UV-44>APLRG1,N7UV-6,RFONLY");
    reply("unknown route uses beacon fallback", "malformed",
          "N7UV-44>APLRG1,WIDE1-1,RFONLY");
    if (APRS_RETURN_ROUTE::buildLocalReplyHeader(
            "N7UV-44", {}, "WIDE1-1", false) != "N7UV-44>APLRG1,WIDE1-1") {
        std::printf("FAIL  Internet sender fallback permits gating\n");
        ++failures;
    }

    check("direct reception", "N7UV-4>APLRT1,WIDE1-1,WIDE2-2:>test", true, "");
    check("canonical two-hop route",
          "N7UV-4>APLRT1,N7UV-6,SOMTNP*,WIDE2-1:>test",
          true, "SOMTNP,N7UV-6");
    check("legacy stars on every used hop",
          "N7UV-4>APLRT1,N7UV-6*,SOMTNP*,WIDE2-1:>test",
          true, "SOMTNP,N7UV-6");
    check("unconsumed regional alias excluded",
          "F4MLV-MC>APLRG1,F4MLV-2*,ARIEG2-1:>test",
          true, "F4MLV-2");
    check("own iGate identity excluded",
          "N7UV-4>APLRT1,N7UV-6,N7UV-44*:>test",
          true, "N7UV-6");
    check("own tactical digi identity excluded",
          "N7UV-4>APLRT1,N7UV-6,TSRXBX*:>test",
          true, "N7UV-6");
    check("Internet-only components excluded",
          "N7UV-4>APLRT1,TCPIP,qAR,N7UV-44*:>test",
          true, "");
    check("duplicate relay rejected",
          "N7UV-4>APLRT1,SOMTNP,SOMTNP*:>test",
          false, "");
    check("malformed packet rejected", "N7UV-4 APLRT1:>test", false, "");

    const std::string internetMessage =
        "WHOIS>APRS,TCPIP*,qAC,T2SERVER::N7UV-4  :response{42";
    const std::string routed = APRS_RETURN_ROUTE::buildThirdPartyMessage(
        "N7UV-44", "SOMTNP,N7UV-6", internetMessage);
    const std::string routedExpected =
        "N7UV-44>APLRG1,SOMTNP,N7UV-6:}WHOIS>APRS,TCPIP,N7UV-44*::N7UV-4  :response{42";
    if (routed != routedExpected) {
        std::printf("FAIL  routed third-party message\n        expected: %s\n        got     : %s\n",
                    routedExpected.c_str(), routed.c_str());
        ++failures;
    } else {
        std::printf("PASS  routed third-party message\n");
    }

    const std::string direct = APRS_RETURN_ROUTE::buildThirdPartyMessage(
        "N7UV-44", "", internetMessage);
    const std::string directExpected =
        "N7UV-44>APLRG1:}WHOIS>APRS,TCPIP,N7UV-44*::N7UV-4  :response{42";
    if (direct != directExpected) {
        std::printf("FAIL  direct third-party message\n");
        ++failures;
    } else {
        std::printf("PASS  direct third-party message\n");
    }

    if (!APRS_RETURN_ROUTE::buildThirdPartyMessage(
            "N7UV-44", "SOMTNP,N7UV-6", "not an APRS message").empty()) {
        std::printf("FAIL  malformed Internet message rejected\n");
        ++failures;
    } else {
        std::printf("PASS  malformed Internet message rejected\n");
    }

    if (failures != 0) {
        std::printf("\n%d return-route test(s) failed\n", failures);
        return 1;
    }
    std::printf("\nAll return-route tests passed\n");
    return 0;
}
