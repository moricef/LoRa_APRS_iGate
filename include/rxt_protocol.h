#ifndef RXT_PROTOCOL_H_
#define RXT_PROTOCOL_H_

#include <cstddef>
#include <string>
#include <vector>

namespace RXT_Protocol {

    constexpr size_t LORA_MAX_PAYLOAD_BYTES = 255;
    constexpr size_t LORA_APRS_PREFIX_BYTES = 3;
    bool fitsLoRaPayload(size_t packetBytes);
    // Leaves the original packet (including existing tuples) intact if the
    // new tuple does not fit. Oversize originals must be rejected by caller.
    std::string attachTrailerWithinLimit(const std::string& packet, const std::string& newTuple);

    std::string attachTrailer(const std::string& packet, const std::string& newTuple);
    std::string stripTrailer(const std::string& packet, std::string* outTuples = nullptr);
    std::vector<std::string> usedPathNodes(
        const std::string& packet,
        const std::string& regionalAliases = "");
    bool isAprsMessage(const std::string& packet);

}

#endif
