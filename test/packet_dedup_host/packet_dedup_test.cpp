#include "packet_dedup.h"
#include "local_message_gate.h"

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
    for (char dti : {'`', '\'', '\x1c', '\x1d'}) {
        PACKET_DEDUP::Cache micE;
        const std::string information = std::string(1, dti) + "abcdef>/";
        for (uint8_t consumer : {PACKET_DEDUP::DIGI, PACKET_DEDUP::RETURN_ROUTE}) {
            expect("Mic-E first latitude", micE.claim("SRC", information, consumer, 0, "490350"), true);
            expect("Mic-E different latitude", micE.claim("SRC", information, consumer, 1, "490351"), true);
            expect("Mic-E repeat same latitude", micE.claim("SRC", information, consumer, 2, "490350"), false);
            expect("Mic-E repeat with RXT", micE.claim("SRC", information + "{hABCD}", consumer, 3, "490351"), false);
        }
    }
    PACKET_DEDUP::Cache nonMicE;
    expect("non-Mic-E initial", nonMicE.claim("SRC", ">status", PACKET_DEDUP::DIGI, 0, "APLRG1"), true);
    expect("non-Mic-E behaviour unchanged", nonMicE.claim("SRC", ">status", PACKET_DEDUP::DIGI, 1, "APTEST"), false);
    for (const std::string number : {"1", "12345", "aB9", "MM}AA", "MM}", "1}2"}) {
        expect("valid ACK " + number,
               APRS_MESSAGE::ackText(number) == "ack" + number, true);
    }
    for (const std::string number : {"", "123456", "}AA", "MM}}", "MM}AAA",
                                     "MMM}A", "MM}A!", "A B", "A\nB", "A{B"}) {
        expect("invalid ACK " + number, APRS_MESSAGE::ackText(number).empty(), true);
    }
    LocalMessageGate replyAck;
    replyAck.beginReception();
    expect("reply-ack query executes",
           replyAck.claim("SRC", "TARGET   :?APRSV{MM}AA", true, 0).executeQuery, true);
    replyAck.beginReception();
    auto retry = replyAck.claim("SRC", "TARGET   :?APRSV{MM}BB", true, 1);
    expect("changed piggyback ACK does not execute again", retry.executeQuery, false);
    expect("changed piggyback ACK still acknowledged", retry.acknowledge, true);
    replyAck.beginReception();
    expect("empty piggyback ACK remains same query",
           replyAck.claim("SRC", "TARGET   :?APRSV{MM}", true, 2).executeQuery, false);
    replyAck.beginReception();
    expect("new reply-ack number executes",
           replyAck.claim("SRC", "TARGET   :?APRSV{MN}AA", true, 3).executeQuery, true);

    LocalMessageGate local;
    const std::string query = "F4MLV-15 :?TX=?{01";
    local.beginReception();
    auto decision = local.claim("F4MLV-7", query, true, 1000);
    expect("local query first consumer ACK", decision.acknowledge, true);
    expect("local query first consumer executes", decision.executeQuery, true);
    decision = local.claim("F4MLV-7", query, true, 1001);
    expect("same reception second consumer no ACK", decision.acknowledge, false);
    expect("same reception second consumer no execution", decision.executeQuery, false);

    local.beginReception();
    decision = local.claim("F4MLV-7", query, true, 2000);
    expect("RF retry can recover lost ACK", decision.acknowledge, true);
    expect("RF copy or retry does not execute twice", decision.executeQuery, false);

    local.beginReception();
    decision = local.claim("F4MLV-7", "F4MLV-15 :?TX=?{02", true, 2001);
    expect("new message number executes", decision.executeQuery, true);
    local.beginReception();
    decision = local.claim("F4MLV-8", query, true, 2002);
    expect("another sender executes", decision.executeQuery, true);
    local.beginReception();
    decision = local.claim("F4MLV-7", "F4MLV-15 :?APRSV{01", true, 2003);
    expect("different question executes", decision.executeQuery, true);

    local.beginReception();
    decision = local.claim("F4MLV-7", query, true, 26000);
    expect("query suppressed at window boundary", decision.executeQuery, false);
    local.beginReception();
    decision = local.claim("F4MLV-7", query, true, 26001);
    expect("query executes again after window", decision.executeQuery, true);

    LocalMessageGate digiOnly;
    digiOnly.beginReception();
    expect("digi-only unnumbered query executes",
           digiOnly.claim("SRC", "TARGET   :?APRSV", true, 0).executeQuery, true);
    digiOnly.beginReception();
    expect("digi-only unnumbered RF copy suppressed",
           digiOnly.claim("SRC", "TARGET   :?APRSV", true, 1).executeQuery, false);

    LocalMessageGate ordinary;
    ordinary.beginReception();
    decision = ordinary.claim("SRC", "TARGET   :hello{01", false, 0);
    expect("ordinary message can be ACKed", decision.acknowledge, true);
    expect("ordinary message is not a query", decision.executeQuery, false);
    expect("ordinary message second consumer no ACK",
           ordinary.claim("SRC", "TARGET   :hello{01", false, 1).acknowledge, false);
    ordinary.beginReception();
    expect("ordinary message retry can be ACKed",
           ordinary.claim("SRC", "TARGET   :hello{01", false, 2).acknowledge, true);

    PACKET_DEDUP::Cache cache;

    expect("first digi claim",
           cache.claim("F4MLV-2", "payload", PACKET_DEDUP::DIGI, 1000), true);
    expect("same packet available for first return-route observation",
           cache.claim("F4MLV-2", "payload", PACKET_DEDUP::RETURN_ROUTE, 1001), true);
    expect("duplicate digi claim",
           cache.claim("F4MLV-2", "payload", PACKET_DEDUP::DIGI, 1002), false);
    expect("duplicate cannot replace learned return route",
           cache.claim("F4MLV-2", "payload", PACKET_DEDUP::RETURN_ROUTE, 1003), false);
    expect("different source",
           cache.claim("F4MLV-10", "payload", PACKET_DEDUP::DIGI, 1004), true);

    PACKET_DEDUP::Cache spaces;
    expect("plain information", spaces.claim("SRC", "payload", PACKET_DEDUP::DIGI, 0), true);
    expect("trailing space is significant",
           spaces.claim("SRC", "payload ", PACKET_DEDUP::DIGI, 1), true);
    expect("leading space is significant",
           spaces.claim("SRC", " payload", PACKET_DEDUP::DIGI, 2), true);
    const std::string binaryOne("\x1d\0ABC", 5);
    const std::string binaryTwo("\x1d\0ABD", 5);
    expect("binary information", spaces.claim("SRC", binaryOne, PACKET_DEDUP::DIGI, 3), true);
    expect("binary information duplicate",
           spaces.claim("SRC", binaryOne, PACKET_DEDUP::DIGI, 4), false);
    expect("binary information remains exact",
           spaces.claim("SRC", binaryTwo, PACKET_DEDUP::DIGI, 5), true);

    PACKET_DEDUP::Cache rxt;
    expect("first RXT form",
           rxt.claim("SRC", "payload{hABCD}", PACKET_DEDUP::DIGI, 0), true);
    expect("accumulated RXT is same information",
           rxt.claim("SRC", "payload{hABCD*EFGH}", PACKET_DEDUP::DIGI, 1), false);
    expect("clean information matches RXT form",
           rxt.claim("SRC", "payload", PACKET_DEDUP::DIGI, 2), false);
    // Without a path only the v2 shape can be checked: 5, 10 or 15
    // characters in braces are ignored in the key, other braces are not.
    expect("ordinary brace text remains significant",
           rxt.claim("SRC", "payload{hi}", PACKET_DEDUP::DIGI, 3), true);

    PACKET_DEDUP::Cache expiry;
    expect("initial expiry claim", expiry.claim("SRC", "p", PACKET_DEDUP::RETURN_ROUTE, 10), true);
    expect("exact window remains duplicate",
           expiry.claim("SRC", "p", PACKET_DEDUP::RETURN_ROUTE,
                        10 + PACKET_DEDUP::DEFAULT_WINDOW_MS), false);
    expect("after window can be claimed again",
           expiry.claim("SRC", "p", PACKET_DEDUP::RETURN_ROUTE,
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
