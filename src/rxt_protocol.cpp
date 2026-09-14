#include "rxt_protocol.h"

#include <algorithm>

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

}

namespace RXT_Protocol {

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

std::vector<std::string> usedPathNodes(const std::string& packet) {
    std::vector<std::string> pathNodes;
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
            pathNodes.push_back(node);
            if (starred) lastStarredIndex = static_cast<int>(pathNodes.size()) - 1;
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }

    if (lastStarredIndex < 0) return {};
    pathNodes.resize(static_cast<size_t>(lastStarredIndex) + 1);
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
