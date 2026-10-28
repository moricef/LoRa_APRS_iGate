// Frame sizes of the native format and APRS-PPM variants, one TNC2 packet per
// line on stdin. Every coded frame is decoded and compared with the input.
// stdout, one line per packet (bytes, 3-byte LoRa prefix included):
//   plain native_huffman native_ppm_text native_best_text native_raw_source
//   ppm_whole clear_source_ppm_rest
// stderr: totals, savings and computed airtime savings, then check counters.
#include "native_aprs.h"
#include "rxt_protocol.h"
#include "AprsCompress.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* const NAMES[] = {"plain", "native_huffman", "native_ppm_text", "native_best_text",
                             "native_raw_source", "ppm_whole", "clear_source_ppm_rest"};
const int COLUMNS = 7;

// LoRa time on air: BW125, 8-symbol preamble, explicit header, CRC on,
// low data rate optimisation at SF11 and SF12.
double airtime(size_t bytes, int sf, int cr) {
    const int de = sf >= 11 ? 1 : 0;
    const double n = std::ceil((8.0 * bytes - 4 * sf + 28 + 16) / (4.0 * (sf - 2 * de)));
    const double symbols = 8 + std::max(n * (4 + cr), 0.0);
    return (8 + 4.25 + symbols) * std::pow(2.0, sf) / 125000.0;
}

// Size of the source element as the draft 0 encoder writes it: 5 bytes in
// base 40 (6 when the SSID is above 30) when it matches matchCall() in
// native_aprs.cpp, otherwise a raw text element of 1 + length bytes.
size_t encodedSourceSize(const std::string& src) {
    size_t i = 0;
    while (i < src.size() && i < 6 && ((src[i] >= 'A' && src[i] <= 'Z') || (src[i] >= '0' && src[i] <= '9'))) i++;
    if (i == 0) return 1 + src.size();
    int ssid = 0;
    if (i < src.size() && src[i] == '-') {
        size_t j = i + 1;
        while (j < src.size() && j < i + 4 && src[j] >= '0' && src[j] <= '9') j++;
        const size_t len = j - (i + 1);
        if (len == 0 || src[i + 1] == '0') return 1 + src.size();
        ssid = std::atoi(src.substr(i + 1, len).c_str());
        if (ssid > 255) return 1 + src.size();
        i = j;
    }
    if (i != src.size()) return 1 + src.size();
    return ssid > 30 ? 6 : 5;
}

unsigned ppmRefused = 0, ppmMismatch = 0;

size_t ppm(const std::string& s) {
    uint8_t f[255], d[255];
    size_t n = AprsCompress::encodeFrame(reinterpret_cast<const uint8_t*>(s.data()), s.size(), f, sizeof f);
    if (!n) { ppmRefused++; return 0; }
    if (AprsCompress::decodeFrame(f, n, d, sizeof d) != s.size() || memcmp(d, s.data(), s.size())) {
        ppmMismatch++;
        return 0;
    }
    return n;
}

}

int main() {
    std::string line;
    unsigned nativeFailures = 0, packets = 0, oversize = 0;
    double bytes[COLUMNS] = {}, sf12[COLUMNS] = {}, sf7[COLUMNS] = {};
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line.size() > 252) { oversize++; continue; }
        line = RXT_Protocol::stripTrailerShape(line);
        const size_t plain = line.size() + 3;
        const size_t gt = line.find('>');
        if (gt == std::string::npos || line.find(':') < gt) continue;
        size_t size[COLUMNS];
        size[0] = plain;

        for (int mode = 0; mode < 3; mode++) {
            NATIVE_APRS::textMode = mode;
            NATIVE_APRS::lastTextPpm = false;
            std::vector<uint8_t> f;
            std::vector<NATIVE_APRS::RxtTuple> none, back;
            std::string out;
            bool ok = NATIVE_APRS::encode(line, none, f) && f.size() <= 255 &&
                      NATIVE_APRS::decode(f.data(), f.size(), out, back) && out == line;
            if (!ok) nativeFailures++;
            size[1 + mode] = ok ? f.size() : plain;
        }

        // Best-text frame with the source as a raw text element (0b11 lllll +
        // text, already defined by draft 0) instead of its encoded form.
        // Computed, not encoded.
        const std::string src = line.substr(0, gt);
        size[4] = size[3] - encodedSourceSize(src) + 1 + src.size();

        const size_t whole = ppm(line);
        size[5] = whole ? whole : plain;
        // Source and '>' in clear after the radio prefix, rest coded by PPM.
        const size_t rest = ppm(line.substr(gt + 1));
        size[6] = rest ? rest + gt + 1 : plain;

        for (int c = 0; c < COLUMNS; c++) {
            std::cout << size[c] << (c + 1 < COLUMNS ? '\t' : '\n');
            bytes[c] += size[c];
            sf12[c] += airtime(size[c], 12, 1);
            sf7[c] += airtime(size[c], 7, 2);
        }
        packets++;
    }
    std::fprintf(stderr, "%u packets\n%-22s %10s %8s %12s %12s\n", packets, "column", "mean B",
                 "saved", "SF12 CR4/5", "SF7 CR4/6");
    for (int c = 0; packets && c < COLUMNS; c++) {
        std::fprintf(stderr, "%-22s %10.2f %7.1f%% %11.1f%% %11.1f%%\n", NAMES[c], bytes[c] / packets,
                     100 * (1 - bytes[c] / bytes[0]), 100 * (1 - sf12[c] / sf12[0]), 100 * (1 - sf7[c] / sf7[0]));
    }
    std::fprintf(stderr, "native_failures=%u ppm_refused=%u ppm_mismatch=%u oversized_skipped=%u\n",
                 nativeFailures, ppmRefused, ppmMismatch, oversize);
    return nativeFailures || ppmMismatch;
}
