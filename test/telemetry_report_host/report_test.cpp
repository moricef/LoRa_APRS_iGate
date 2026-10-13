#include "telemetry_report.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace TelemetryReport;
    assert(needsSeparateReport(false, '_'));
    assert(needsSeparateReport(true, '_'));
    assert(needsSeparateReport(true, '#'));
    assert(!needsSeparateReport(false, '#'));
    // Actual F1ZDB-13 reception: sequence 1216, battery 4.29 V,
    // RX 116/h, relay 52/h, drop 64/h. Coefficients stay unchanged.
    assert(fromBase91("|.B%b\":!U!a|") == "T#216,429,116,52,64");
    std::string full = "|";
    for (unsigned value : {999u, 420u, 750u, 8280u, 1000u, 0u}) {
        full += char(value / 91 + 33);
        full += char(value % 91 + 33);
    }
    full += '|';
    assert(fromBase91(full) == "T#999,420,750,8280,1000,0");
    assert(fromBase91("|!!!!|") == "T#000,0");
    assert(fromBase91("").empty());
    assert(fromBase91("|!!|").empty());
    assert(fromBase91("|!!!!!|").empty());
    assert(fromBase91("|!! ~|").empty());
    std::cout << fromBase91("|.B%b\":!U!a|") << '\n';
    std::cout << fromBase91(full) << '\n';
}
