// Command-line front end of the native LoRa APRS codec, for corpus analysis.
//
// Input, one packet per line: "<rxt>\t<tnc2 hex>", where <rxt> is empty or
// "index:tuplehex[;index:tuplehex...]" (4-byte tuples).
//
//   encode  -> native frame in hex, or "ERR"
//   check   -> "OK <tnc2 bytes> <native bytes>", "ERR" or "MISMATCH"
//   text    -> free text carried by the frame, in hex
//   decode  -> input "<native hex>", output "<rxt>\t<tnc2 hex>" or "ERR"
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "native_aprs.h"

namespace {

    std::string toHex(const std::string& s) {
        static const char* d = "0123456789abcdef";
        std::string out;
        for (unsigned char c : s) { out += d[c >> 4]; out += d[c & 15]; }
        return out;
    }

    bool fromHex(const std::string& h, std::string& out) {
        if (h.size() % 2) return false;
        out.clear();
        for (size_t i = 0; i < h.size(); i += 2) {
            unsigned v;
            if (sscanf(h.c_str() + i, "%2x", &v) != 1) return false;
            out += static_cast<char>(v);
        }
        return true;
    }

    bool parseRxt(const std::string& spec, std::vector<NATIVE_APRS::RxtTuple>& rxt) {
        rxt.clear();
        size_t start = 0;
        while (start < spec.size()) {
            size_t end = spec.find(';', start);
            std::string item = spec.substr(start, end == std::string::npos ? std::string::npos : end - start);
            size_t colon = item.find(':');
            std::string tuple;
            if (colon == std::string::npos || !fromHex(item.substr(colon + 1), tuple) || tuple.size() != 4) return false;
            NATIVE_APRS::RxtTuple t;
            t.index = static_cast<uint8_t>(std::stoi(item.substr(0, colon)));
            std::copy(tuple.begin(), tuple.end(), t.tuple);
            rxt.push_back(t);
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return true;
    }

    std::string rxtSpec(const std::vector<NATIVE_APRS::RxtTuple>& rxt) {
        std::string s;
        for (const NATIVE_APRS::RxtTuple& t : rxt) {
            if (!s.empty()) s += ";";
            s += std::to_string(t.index) + ":" + toHex(std::string(t.tuple, 4));
        }
        return s;
    }

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: native_aprs_tool encode|check|text|decode < lines\n";
        return 2;
    }
    const std::string mode = argv[1];
    std::string line;
    while (std::getline(std::cin, line)) {
        if (mode == "decode") {
            std::string frame, tnc2;
            std::vector<NATIVE_APRS::RxtTuple> rxt;
            if (!fromHex(line, frame) ||
                !NATIVE_APRS::decode(reinterpret_cast<const uint8_t*>(frame.data()), frame.size(), tnc2, rxt)) {
                std::cout << "ERR\n";
                continue;
            }
            std::cout << rxtSpec(rxt) << "\t" << toHex(tnc2) << "\n";
            continue;
        }
        size_t tab = line.find('\t');
        std::string tnc2;
        std::vector<NATIVE_APRS::RxtTuple> rxt;
        if (tab == std::string::npos || !parseRxt(line.substr(0, tab), rxt) || !fromHex(line.substr(tab + 1), tnc2)) {
            std::cout << "ERR\n";
            continue;
        }
        if (mode == "text") {
            std::cout << toHex(NATIVE_APRS::freeText(tnc2)) << "\n";
            continue;
        }
        std::vector<uint8_t> out;
        if (!NATIVE_APRS::encode(tnc2, rxt, out)) {
            std::cout << "ERR\n";
            continue;
        }
        if (mode == "encode") {
            std::cout << toHex(std::string(out.begin(), out.end())) << "\n";
            continue;
        }
        std::string back;
        std::vector<NATIVE_APRS::RxtTuple> backRxt;
        bool ok = NATIVE_APRS::decode(out.data(), out.size(), back, backRxt) && back == tnc2 &&
                  rxtSpec(backRxt) == rxtSpec(rxt);
        std::cout << (ok ? "OK " + std::to_string(tnc2.size()) + " " + std::to_string(out.size()) : "MISMATCH") << "\n";
    }
    return 0;
}
