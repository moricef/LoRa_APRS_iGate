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
    // A single tuple for two relays sharing a fingerprint has no known place
    // in the mask: the packet must stay in text form.
    std::vector<NATIVE_APRS::RxtTuple> second = {tuple(1, "bbbb")};
    expect("fromText refuses a tuple that two relays can have produced",
           !NATIVE_APRS::fromText(NATIVE_APRS::toText(shared, second), "", back, rxt));

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

void testNativeRelayIdentity() {
    // Both calls have ID 'C'. Only the second measured the reception.
    const std::string received = "F4MLV-9>APLRG1,WIDE1*,F4MLV-18,F6DEV-10*,WIDE2-1:>test {world}";
    NATIVE_APRS::RxtState state;
    state.native = true;
    std::vector<uint8_t> frame;
    expect("encode exact second owner despite fingerprint collision",
           NATIVE_APRS::encode(received, {tuple(2, "bbbb")}, frame));
    expect("decode native relay snapshot",
           NATIVE_APRS::decode(frame.data(), frame.size(), state.packet, state.tuples));
    expect("native reception attributes second colliding relay exactly",
           NATIVE_APRS::tupleOwners(state.packet, state.tuples) == std::vector<int>{1});

    // Exercise the queued copy after another reception replaces live state.
    const NATIVE_APRS::RxtState queued = state;
    state.packet = "OTHER>APRS:>unrelated";
    state.tuples.clear();
    const std::string relayed = "F4MLV-9>APLRG1,F4MLV-18,F6DEV-10,F4MLV-10*:>test {world}";
    std::vector<NATIVE_APRS::RxtTuple> remapped;
    expect("removing consumed alias preserves queued measurement owner",
           NATIVE_APRS::remapRxt(queued, relayed, "", remapped) &&
           sameRxt(remapped, {tuple(1, "bbbb")}));
    expect("append local measurement using full callsign",
           NATIVE_APRS::appendRelayTuple(relayed, "F4MLV-10", "cccc", "", remapped));
    std::string back;
    std::vector<NATIVE_APRS::RxtTuple> decoded;
    expect("native relay preserves second owner and new local measurement",
           NATIVE_APRS::encode(relayed, remapped, frame) &&
           NATIVE_APRS::decode(frame.data(), frame.size(), back, decoded) && back == relayed &&
           sameRxt(decoded, {tuple(1, "bbbb"), tuple(2, "cccc")}));
    expect("a relay cannot append a tuple under another callsign",
           !NATIVE_APRS::appendRelayTuple(relayed, "F4MLV-18", "dddd", "", remapped));
    expect("snapshot cannot be applied to unrelated APRS data",
           !NATIVE_APRS::remapRxt(queued, "F4MLV-9>APLRG1,F6DEV-10*:>other", "", remapped));
    expect("removing measured relay does not transfer its measurement",
           NATIVE_APRS::remapRxt(queued, "F4MLV-9>APLRG1,F4MLV-18,F4MLV-10*:>test {world}", "", remapped) && remapped.empty());

    NATIVE_APRS::RxtState repeated;
    repeated.native = true;
    repeated.packet = "SRC>APRS,F6DEV-10,F6DEV-10*:>test";
    repeated.tuples = {tuple(1, "bbbb")};
    expect("repeated full callsigns preserve their occurrence",
           NATIVE_APRS::remapRxt(repeated, "SRC>APRS,F6DEV-10,F6DEV-10,NEW*:>test", "", remapped) &&
           sameRxt(remapped, {tuple(1, "bbbb")}));
    expect("deleting one repeated callsign refuses to guess",
           !NATIVE_APRS::remapRxt(repeated, "SRC>APRS,F6DEV-10,NEW*:>test", "", remapped));

    NATIVE_APRS::RxtState many;
    many.native = true;
    many.packet = "SRC>APRS,A1,B1,C1,D1*:>test";
    many.tuples = {tuple(0, "aaaa"), tuple(1, "bbbb"), tuple(2, "cccc"), tuple(3, "dddd")};
    expect("native snapshot retains more than three measurements",
           NATIVE_APRS::remapRxt(many, many.packet, "", remapped) && sameRxt(remapped, many.tuples));
    expect("raw native metrics export retains all measurements",
           NATIVE_APRS::rawTuples(many.packet, many.tuples).size() == 20);

    // A legitimate APRS comment resembling a text trailer is never split
    // when the packet was decoded natively.
    NATIVE_APRS::RxtState comment;
    comment.native = true;
    comment.packet = "SRC>APRS,F4MLV-18,F6DEV-10*:>user {Cbbbb}";
    comment.tuples = {tuple(1, "aaaa")};
    const std::string withComment = "SRC>APRS,F4MLV-18,F6DEV-10,F4MLV-10*:>user {Cbbbb}";
    expect("native relay preserves a trailer-looking user comment",
           NATIVE_APRS::remapRxt(comment, withComment, "", remapped) &&
           NATIVE_APRS::encode(withComment, remapped, frame) &&
           NATIVE_APRS::decode(frame.data(), frame.size(), back, decoded) && back == withComment &&
           sameRxt(decoded, {tuple(1, "aaaa")}));
    expect("APRS-IS path changes keep native user comment opaque",
           NATIVE_APRS::samePacketContent(comment.packet,
               "SRC>APRS,F4MLV-18,F6DEV-10*,qAR,F4MLV-10:>user {Cbbbb}"));
    expect("content guard distinguishes changed destination",
           !NATIVE_APRS::samePacketContent(comment.packet,
               "SRC>OTHER,F6DEV-10*:>user {Cbbbb}"));
    remapped.clear();
    expect("local measurement accepts tactical relay identity",
           NATIVE_APRS::appendRelayTuple("SRC>APRS,F6DEV-10,PINAL*:>test", "PINAL", "aaaa", "", remapped));
}

}

int main(int argc, char** argv) {
    testVectors(argc > 1 ? argv[1] : "vectors.txt");
    testRejects();
    testHuffman();
    testToText();
    testFromText();
    testNativeRelayIdentity();
    if (failures) {
        std::cerr << failures << " of " << checks << " checks failed\n";
        return 1;
    }
    std::cout << "native APRS host tests passed (" << checks << " checks)\n";
    return 0;
}
