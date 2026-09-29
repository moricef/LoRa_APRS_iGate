#include "native_aprs.h"
#include "rxt_protocol.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;
int checks = 0;

void expect(const std::string& name, bool ok) {
    checks++;
    if (ok) return;
    std::cerr << "FAIL " << name << '\n';
    failures++;
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

std::vector<NATIVE_APRS::RxtTuple> parseRxt(const std::string& spec) {
    std::vector<NATIVE_APRS::RxtTuple> rxt;
    size_t start = 0;
    while (start < spec.size()) {
        size_t end = spec.find(';', start);
        std::string item = spec.substr(start, end == std::string::npos ? std::string::npos : end - start);
        size_t colon = item.find(':');
        std::string tuple;
        fromHex(item.substr(colon + 1), tuple);
        NATIVE_APRS::RxtTuple t;
        t.index = static_cast<uint8_t>(std::stoi(item.substr(0, colon)));
        for (int k = 0; k < 4; k++) t.tuple[k] = tuple[k];
        rxt.push_back(t);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return rxt;
}

bool sameRxt(const std::vector<NATIVE_APRS::RxtTuple>& a, const std::vector<NATIVE_APRS::RxtTuple>& b) {
    if (a.size() != b.size()) return false;
    for (size_t k = 0; k < a.size(); k++) {
        if (a[k].index != b[k].index || std::string(a[k].tuple, 4) != std::string(b[k].tuple, 4)) return false;
    }
    return true;
}

void testVectors(const char* path) {
    std::ifstream in(path, std::ios::binary);
    expect("vector file opens", in.good());
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t t1 = line.find('\t'), t2 = line.find('\t', t1 + 1);
        std::string tnc2, native;
        fromHex(line.substr(t1 + 1, t2 - t1 - 1), tnc2);
        fromHex(line.substr(t2 + 1), native);
        std::vector<NATIVE_APRS::RxtTuple> rxt = parseRxt(line.substr(0, t1));
        std::string label = "vector " + std::to_string(++n) + " " + tnc2.substr(0, 40);

        std::vector<uint8_t> out;
        expect(label + ": encode", NATIVE_APRS::encode(tnc2, rxt, out) &&
                                   std::string(out.begin(), out.end()) == native);
        std::string back;
        std::vector<NATIVE_APRS::RxtTuple> backRxt;
        expect(label + ": decode", NATIVE_APRS::decode(reinterpret_cast<const uint8_t*>(native.data()),
                                                       native.size(), back, backRxt) &&
                                   back == tnc2 && sameRxt(backRxt, rxt));
        // Every truncation of a valid frame must be rejected or decode to something else.
        for (size_t len = 0; len < native.size(); len++) {
            std::string cut;
            std::vector<NATIVE_APRS::RxtTuple> cutRxt;
            if (NATIVE_APRS::decode(reinterpret_cast<const uint8_t*>(native.data()), len, cut, cutRxt) && cut == tnc2 &&
                sameRxt(cutRxt, rxt)) {
                expect(label + ": truncated to " + std::to_string(len) + " decodes as the original", false);
            }
        }
    }
    expect("vectors present", n > 40);
}

void testRejects() {
    std::vector<uint8_t> out;
    std::vector<NATIVE_APRS::RxtTuple> none;
    expect("no colon", !NATIVE_APRS::encode("F4MLV-9>APLRG1", none, out));
    expect("no '>'", !NATIVE_APRS::encode("F4MLV-9:>test", none, out));
    std::string longPath = "F4MLV-9>APLRG1";
    for (int k = 0; k < 16; k++) longPath += ",WIDE1-1";
    expect("path longer than 15", !NATIVE_APRS::encode(longPath + ":>x", none, out));
    expect("raw element longer than 31",
           !NATIVE_APRS::encode("F4MLV-9>APLRG1,abcdefghijklmnopqrstuvwxyz0123456:>x", none, out));

    NATIVE_APRS::RxtTuple t{0, {'!', '!', '!', '!'}};
    expect("tuple on a path with no element", !NATIVE_APRS::encode("F4MLV-9>APLRG1:>x", {t}, out));
    t.tuple[0] = '{';
    expect("tuple character out of range", !NATIVE_APRS::encode("F4MLV-9>APLRG1,F4MLV-15*:>x", {t}, out));
    t.tuple[0] = '!';
    expect("duplicate tuple index", !NATIVE_APRS::encode("F4MLV-9>APLRG1,F4MLV-15*:>x", {t, t}, out));

    std::string tnc2;
    std::vector<NATIVE_APRS::RxtTuple> rxt;
    const uint8_t v1[] = {'<', 0xFF, 0x01, 0x00, 0x80, 0, 0, 0, 1};
    expect("text frame prefix is not native", !NATIVE_APRS::decode(v1, sizeof(v1), tnc2, rxt));
}

NATIVE_APRS::RxtTuple tuple(uint8_t index, const char* t) {
    NATIVE_APRS::RxtTuple r;
    r.index = index;
    for (int k = 0; k < 4; k++) r.tuple[k] = t[k];
    return r;
}

void testToText() {
    const std::string tnc2 = "F4MLV-9>APLRG1,F4MLV-15*,F4MLV-10*:>test";
    std::string text = NATIVE_APRS::toText(tnc2, {tuple(0, "jf$?"), tuple(1, "cn/?")});
    std::string expected = tnc2 + "{" + RXT_Protocol::fingerprint("F4MLV-15") + "jf$?" +
                           RXT_Protocol::fingerprint("F4MLV-10") + "cn/?}";
    expect("toText builds the v2 trailer", text == expected);
    std::string tuples;
    expect("toText trailer is stripped back to the packet",
           RXT_Protocol::stripTrailer(text, &tuples) == tnc2 && tuples.size() == 10);
    expect("toText without tuples", NATIVE_APRS::toText(tnc2, {}) == tnc2);

    const std::string four = "F4MLV-9>APLRG1,A1*,B1*,C1*,D1*:>test";
    expect("toText drops more than three tuples",
           NATIVE_APRS::toText(four, {tuple(0, "!!!!"), tuple(1, "!!!!"), tuple(2, "!!!!"), tuple(3, "!!!!")}) == four);

    const std::string unused = "F4MLV-9>APLRG1,F4MLV-15*,F4MLV-10:>test";
    expect("toText drops a tuple on an unused element", NATIVE_APRS::toText(unused, {tuple(1, "!!!!")}) == unused);
    expect("toText drops an out-of-range character", NATIVE_APRS::toText(tnc2, {tuple(0, "!!!{")}) == tnc2);
}

void testFromText() {
    const std::string tnc2 = "F4MLV-9>APLRG1,F4MLV-15*,F4MLV-10*:>test";
    std::vector<NATIVE_APRS::RxtTuple> in = {tuple(0, "jf$?"), tuple(1, "cn/?")};
    std::string back;
    std::vector<NATIVE_APRS::RxtTuple> rxt;
    expect("fromText inverts toText",
           NATIVE_APRS::fromText(NATIVE_APRS::toText(tnc2, in), "", back, rxt) && back == tnc2 && sameRxt(rxt, in));
    expect("fromText without trailer", NATIVE_APRS::fromText(tnc2, "", back, rxt) && back == tnc2 && rxt.empty());

    const std::string comment = "F4MLV-9>APLRG1,F4MLV-15*:>hello {world}";
    expect("fromText keeps a comment in braces",
           NATIVE_APRS::fromText(comment, "", back, rxt) && back == comment && rxt.empty());

    // Shared fingerprint (F4MLV-2 and F1ZDB-10 both give '*'): tuples follow path order.
    const std::string shared = "F4MLV-9>APLRG1,F4MLV-2*,F1ZDB-10*:>test";
    std::vector<NATIVE_APRS::RxtTuple> two = {tuple(0, "aaaa"), tuple(1, "bbbb")};
    expect("fromText resolves a shared fingerprint in path order",
           NATIVE_APRS::fromText(NATIVE_APRS::toText(shared, two), "", back, rxt) && back == shared && sameRxt(rxt, two));

    std::vector<uint8_t> frame;
    std::string decoded;
    std::vector<NATIVE_APRS::RxtTuple> decodedRxt;
    expect("text -> native -> text keeps the trailer",
           NATIVE_APRS::fromText(NATIVE_APRS::toText(tnc2, in), "", back, rxt) &&
           NATIVE_APRS::encode(back, rxt, frame) &&
           NATIVE_APRS::decode(frame.data(), frame.size(), decoded, decodedRxt) &&
           NATIVE_APRS::toText(decoded, decodedRxt) == NATIVE_APRS::toText(tnc2, in));
}

void testHuffman() {
    std::string all;
    for (int c = 0; c < 256; c++) all += static_cast<char>(c);
    std::vector<uint8_t> coded = NATIVE_APRS::huffmanEncode(all);
    std::string back;
    expect("Huffman round-trips every byte value",
           NATIVE_APRS::huffmanDecode(coded.data(), coded.size(), all.size(), back) && back == all);
    expect("Huffman rejects a truncated stream",
           !NATIVE_APRS::huffmanDecode(coded.data(), coded.size() / 2, all.size(), back));
}

}

int main(int argc, char** argv) {
    testVectors(argc > 1 ? argv[1] : "vectors.txt");
    testRejects();
    testHuffman();
    testToText();
    testFromText();
    if (failures) {
        std::cerr << failures << " of " << checks << " checks failed\n";
        return 1;
    }
    std::cout << "native APRS host tests passed (" << checks << " checks)\n";
    return 0;
}
