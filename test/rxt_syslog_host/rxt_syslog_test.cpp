#include "rxt_syslog.h"

#include <cstdio>
#include <cstdlib>
#include <string>

static int failures = 0;

static void expect(const std::string& got, const std::string& want, const char* name) {
    if (got != want) {
        std::printf("FAIL %s\n  got:  \"%s\"\n  want: \"%s\"\n", name, got.c_str(), want.c_str());
        failures++;
    } else {
        std::printf("ok   %s\n", name);
    }
}

int main() {
    using RXT_Syslog::Hop;
    // Path N0CALL-1*,N0CALL-2*: newest first, as getDecodedRxtMetrics() returns it.
    expect(RXT_Syslog::format("N0CALL-7", {
               {"N0CALL-1", "N0CALL-2", true, -112, -8.25f, 340, 2610},
               {"N0CALL-7", "N0CALL-1", true, -95, 6.5f, -210, 1250}}),
           "RXT / N0CALL-7 / 0 N0CALL-7>N0CALL-1 -95dBm 6.50dB -210Hz 1250ms; 1 N0CALL-1>N0CALL-2 -112dBm -8.25dB 340Hz 2610ms",
           "two measured hops in path order");
    expect(RXT_Syslog::format("N0CALL-7", {
               {"N0CALL-2", "N0CALL-3", true, -101, 1.0f, 12, 900},
               {"N0CALL-1", "N0CALL-2", false, 0, 0.0f, 0, 0},
               {"N0CALL-7", "N0CALL-1", true, -95, 6.5f, -210, 1250}}),
           "RXT / N0CALL-7 / 0 N0CALL-7>N0CALL-1 -95dBm 6.50dB -210Hz 1250ms; 1 N0CALL-1>N0CALL-2 NA; 2 N0CALL-2>N0CALL-3 -101dBm 1.00dB 12Hz 900ms",
           "unmeasured hop kept as NA");
    expect(RXT_Syslog::format("N0CALL-7", {
               {"N0CALL-7", "N0CALL-1", true, -95, 6.5f, -210, 1250},
               {"UNKNOWN", "RXT_NODE_2", true, -120, -15.5f, -4, 3000}}),
           "RXT / N0CALL-7 / 0 N0CALL-7>N0CALL-1 -95dBm 6.50dB -210Hz 1250ms; ? UNKNOWN>RXT_NODE_2 -120dBm -15.50dB -4Hz 3000ms",
           "unattributed tuple written with index ?");
    expect(RXT_Syslog::format("N0CALL-7", {{"N0CALL-7", "N0CALL-1", false, 0, 0.0f, 0, 0}}), "",
           "no line without a measurement");
    expect(RXT_Syslog::format("N0CALL-7", {}), "", "no line without hops");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
