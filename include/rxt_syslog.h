#ifndef RXT_SYSLOG_H_
#define RXT_SYSLOG_H_

#include <cstdio>
#include <string>
#include <vector>

// Text of the syslog line that follows an "RX /" line when the received
// packet carried RXT tuples. The packet itself is logged without its RXT
// trailer, as before; this separate line leaves existing syslog parsers
// unaffected.
//
//   RXT / <source> / <index> <from>>to> <rssi>dBm <snr>dB <fo>Hz <tth>ms; ...
//
// <index> is the index in the path of the receiving element, counted from 0;
// "?" when the tuple could not be attributed to a path element. A hop of the
// path that no RXT station measured is written "<index> <from>><to> NA".
namespace RXT_Syslog {

    struct Hop {
        std::string from;
        std::string to;
        bool hasData;
        int rssi;
        float snr;
        int fo;
        unsigned long tth;
    };

    // hops in the order returned by LoRa_Utils::getDecodedRxtMetrics():
    // path elements newest first, then tuples attributed to no element
    // (from == "UNKNOWN"). Returns "" when no hop carries a measurement.
    inline std::string format(const std::string& source, const std::vector<Hop>& hops) {
        bool measured = false;
        size_t inPath = 0;
        for (const Hop& hop : hops) {
            measured = measured || hop.hasData;
            if (hop.from != "UNKNOWN") inPath++;
        }
        if (!measured) return "";

        std::vector<std::string> parts;
        auto add = [&parts](const std::string& index, const Hop& hop) {
            std::string part = index + " " + hop.from + ">" + hop.to;
            if (!hop.hasData) {
                part += " NA";
            } else {
                char values[64];
                snprintf(values, sizeof(values), " %ddBm %.2fdB %dHz %lums", hop.rssi, hop.snr, hop.fo, hop.tth);
                part += values;
            }
            parts.push_back(part);
        };
        for (size_t k = inPath; k-- > 0;) add(std::to_string(inPath - 1 - k), hops[k]);
        for (size_t k = inPath; k < hops.size(); k++) add("?", hops[k]);

        std::string line = "RXT / " + source + " / ";
        for (size_t i = 0; i < parts.size(); i++) {
            if (i > 0) line += "; ";
            line += parts[i];
        }
        return line;
    }

}

#endif
