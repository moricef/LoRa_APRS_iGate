#include "packet_dedup.h"

#include <cstdint>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(const std::string& name, bool actual, bool expected) {
    if (actual == expected) return;
    std::cerr << "FAIL " << name << ": expected " << expected
              << ", got " << actual << '\n';
    failures++;
}

void expectDifferent(const std::string& name, uint64_t left, uint64_t right) {
    if (left != right) return;
    std::cerr << "FAIL " << name << ": fingerprints are equal\n";
    failures++;
}

} // namespace

int main() {
    PACKET_DEDUP::Cache cache;

    expect("first APRS-IS claim",
           cache.claim("F4MLV-2", "payload", PACKET_DEDUP::APRSIS, 1000), true);
    expect("same packet still available to digi",
           cache.claim("F4MLV-2", "payload", PACKET_DEDUP::DIGI, 1001), true);
    expect("duplicate APRS-IS claim",
           cache.claim("F4MLV-2", "payload", PACKET_DEDUP::APRSIS, 1002), false);
    expect("duplicate digi claim",
           cache.claim("F4MLV-2", "payload", PACKET_DEDUP::DIGI, 1003), false);
    expect("different source",
           cache.claim("F4MLV-10", "payload", PACKET_DEDUP::APRSIS, 1004), true);

    PACKET_DEDUP::Cache spaces;
    expect("plain information", spaces.claim("SRC", "payload", PACKET_DEDUP::APRSIS, 0), true);
    expect("trailing space is significant",
           spaces.claim("SRC", "payload ", PACKET_DEDUP::APRSIS, 1), true);
    expect("leading space is significant",
           spaces.claim("SRC", " payload", PACKET_DEDUP::APRSIS, 2), true);
    const std::string binaryOne("\x1d\0ABC", 5);
    const std::string binaryTwo("\x1d\0ABD", 5);
    expect("binary information", spaces.claim("SRC", binaryOne, PACKET_DEDUP::APRSIS, 3), true);
    expect("binary information duplicate",
           spaces.claim("SRC", binaryOne, PACKET_DEDUP::APRSIS, 4), false);
    expect("binary information remains exact",
           spaces.claim("SRC", binaryTwo, PACKET_DEDUP::APRSIS, 5), true);

    PACKET_DEDUP::Cache rxt;
    expect("first RXT form",
           rxt.claim("SRC", "payload{ABCD}", PACKET_DEDUP::DIGI, 0), true);
    expect("accumulated RXT is same information",
           rxt.claim("SRC", "payload{ABCDEFGH}", PACKET_DEDUP::DIGI, 1), false);
    expect("clean information matches RXT form",
           rxt.claim("SRC", "payload", PACKET_DEDUP::DIGI, 2), false);
    expect("ordinary brace text remains significant",
           rxt.claim("SRC", "payload{hello}", PACKET_DEDUP::DIGI, 3), true);

    PACKET_DEDUP::Cache expiry;
    expect("initial expiry claim", expiry.claim("SRC", "p", PACKET_DEDUP::APRSIS, 10), true);
    expect("exact window remains duplicate",
           expiry.claim("SRC", "p", PACKET_DEDUP::APRSIS,
                        10 + PACKET_DEDUP::DEFAULT_WINDOW_MS), false);
    expect("after window can be claimed again",
           expiry.claim("SRC", "p", PACKET_DEDUP::APRSIS,
                        11 + PACKET_DEDUP::DEFAULT_WINDOW_MS), true);

    PACKET_DEDUP::Cache wrap;
    const uint32_t nearWrap = UINT32_MAX - 100;
    expect("initial wrap claim", wrap.claim("SRC", "p", PACKET_DEDUP::DIGI, nearWrap), true);
    expect("millis wrap within window",
           wrap.claim("SRC", "p", PACKET_DEDUP::DIGI, 100), false);
    expect("millis wrap after window",
           wrap.claim("SRC", "p", PACKET_DEDUP::DIGI,
                      PACKET_DEDUP::DEFAULT_WINDOW_MS + 100), true);

    PACKET_DEDUP::Cache bounded(25000, 2);
    expect("bounded first", bounded.claim("A", "1", PACKET_DEDUP::DIGI, 0), true);
    expect("bounded second", bounded.claim("B", "2", PACKET_DEDUP::DIGI, 1), true);
    expect("bounded third", bounded.claim("C", "3", PACKET_DEDUP::DIGI, 2), true);
    expect("oldest entry evicted", bounded.claim("A", "1", PACKET_DEDUP::DIGI, 3), true);

    PACKET_DEDUP::Cache disabled(25000, 0);
    expect("zero capacity refuses claim",
           disabled.claim("SRC", "p", PACKET_DEDUP::DIGI, 0), false);
    expect("zero destination refuses claim",
           cache.claim("SRC", "p", 0, 2000), false);

    expectDifferent("source and information boundary",
                    PACKET_DEDUP::fingerprint("AB", "C"),
                    PACKET_DEDUP::fingerprint("A", "BC"));

    if (failures != 0) return 1;
    std::cout << "packet dedup tests passed\n";
    return 0;
}
