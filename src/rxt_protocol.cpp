#include "rxt_protocol.h"

#include <algorithm>
#include <cctype>

namespace {

bool isRxtContent(const std::string& content) {
    if (content.empty() || content.size() > 12 || content.size() % 4 != 0) return false;
    for (char c : content) {
        if (c < 33 || c > 122) return false;
    }
    return true;
}

size_t trailerStart(const std::string& packet) {
    if (packet.size() <= 6 || packet.back() != '}') return std::string::npos;
    size_t lowerBound = packet.size() > 20 ? packet.size() - 20 : 0;
    size_t start = packet.rfind('{', packet.size() - 2);
    return start != std::string::npos && start > lowerBound ? start : std::string::npos;
}

bool isAliasSeparator(char character) {
    return character == ' ' || character == ',' || character == '\t' ||
           character == '\r' || character == '\n';
}

bool matchesNumberedAlias(const std::string& upper, const std::string& alias) {
    if (alias.empty() || upper.compare(0, alias.size(), alias) != 0) return false;
    if (upper.size() == alias.size()) return true;

    size_t dash = upper.find('-', alias.size() + 1);
    if (dash == std::string::npos || dash + 1 >= upper.size()) return false;
    for (size_t i = alias.size(); i < dash; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(upper[i]))) return false;
    }
    for (size_t i = dash + 1; i < upper.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(upper[i]))) return false;
    }
    return dash > alias.size();
}

bool isRoutingAlias(const std::string& node, const std::string& regionalAliases) {
    std::string upper = node;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    if (upper == "RELAY") return true;

    if (matchesNumberedAlias(upper, "WIDE") || matchesNumberedAlias(upper, "TRACE")) {
        return true;
    }

    size_t start = 0;
    while (start < regionalAliases.size()) {
        while (start < regionalAliases.size() && isAliasSeparator(regionalAliases[start])) ++start;
        if (start >= regionalAliases.size()) break;
        size_t end = start;
        while (end < regionalAliases.size() && !isAliasSeparator(regionalAliases[end])) ++end;
        std::string alias = regionalAliases.substr(start, end - start);
        std::transform(alias.begin(), alias.end(), alias.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        if (matchesNumberedAlias(upper, alias)) return true;
        start = end + 1;
    }
    return false;
}

}

namespace RXT_Protocol {

bool fitsLoRaPayload(size_t packetBytes) {
    return packetBytes <= LORA_MAX_PAYLOAD_BYTES - LORA_APRS_PREFIX_BYTES;
}

std::string attachTrailerWithinLimit(const std::string& packet, const std::string& newTuple) {
    if (!fitsLoRaPayload(packet.size())) return packet;
    const std::string candidate = attachTrailer(packet, newTuple);
    return fitsLoRaPayload(candidate.size()) ? candidate : packet;
}

std::string attachTrailer(const std::string& packet, const std::string& newTuple) {
    size_t start = trailerStart(packet);
    if (start == std::string::npos) return packet + "{" + newTuple + "}";

    std::string content = packet.substr(start + 1, packet.size() - start - 2);
    if (!isRxtContent(content)) {
        return packet + "{" + newTuple + "}";
    }
    if (content.size() >= 12) return packet;
    return packet.substr(0, start) + "{" + content + newTuple + "}";
}

std::string stripTrailer(const std::string& packet, std::string* outTuples) {
    size_t start = trailerStart(packet);
    if (start != std::string::npos) {
        std::string content = packet.substr(start + 1, packet.size() - start - 2);
        if (isRxtContent(content)) {
            if (outTuples != nullptr) *outTuples = content;
            return packet.substr(0, start);
        }
    }
    if (outTuples != nullptr) outTuples->clear();
    return packet;
}

std::vector<std::string> usedPathNodes(const std::string& packet,
                                       const std::string& regionalAliases) {
    std::vector<std::string> pathElements;
    size_t gt = packet.find('>');
    size_t colon = packet.find(':', gt == std::string::npos ? 0 : gt + 1);
    size_t comma = packet.find(',', gt == std::string::npos ? 0 : gt + 1);
    if (gt == std::string::npos || colon == std::string::npos ||
        comma == std::string::npos || comma > colon) return {};

    std::string path = packet.substr(comma + 1, colon - comma - 1);
    int lastStarredIndex = -1;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find(',', start);
        std::string node = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
        bool starred = node.find('*') != std::string::npos;
        node.erase(std::remove(node.begin(), node.end(), '*'), node.end());
        if (!node.empty()) {
            pathElements.push_back(node);
            if (starred) lastStarredIndex = static_cast<int>(pathElements.size()) - 1;
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }

    if (lastStarredIndex < 0) return {};
    pathElements.resize(static_cast<size_t>(lastStarredIndex) + 1);

    // WIDEn-N/TRACE aliases describe routing work, not physical transmitters.
    // Some LoRa digis leave a consumed alias starred alongside their own
    // callsign; retaining it would invent links such as WIDE2-2 -> F4MLV-10.
    std::vector<std::string> pathNodes;
    for (const std::string& node : pathElements) {
        if (!isRoutingAlias(node, regionalAliases)) pathNodes.push_back(node);
    }
    return pathNodes;
}

bool isAprsMessage(const std::string& packet) {
    std::string current = packet;

    // A third-party packet starts its information field with '}' followed by
    // another complete TNC2 frame. Unwrap each level before deciding which
    // APRS data type is actually being carried.
    while (true) {
        size_t gt = current.find('>');
        size_t colon = current.find(':', gt == std::string::npos ? 0 : gt + 1);
        if (gt == std::string::npos || colon == std::string::npos || colon + 1 >= current.size()) {
            return false;
        }

        char dataType = current[colon + 1];
        if (dataType == ':') return true;
        if (dataType != '}') return false;

        current = current.substr(colon + 2);
    }
}

}
