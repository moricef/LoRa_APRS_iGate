#include "aprs_return_route.h"

#include <algorithm>
#include <cctype>

namespace {

std::string upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

std::vector<std::string> split(const std::string& value, char separator) {
    std::vector<std::string> result;
    size_t start = 0;
    while (start <= value.size()) {
        const size_t end = value.find(separator, start);
        result.push_back(value.substr(start, end == std::string::npos
                                                ? std::string::npos
                                                : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return result;
}

bool isDecimal(const std::string& value) {
    if (value.empty()) return false;
    for (const unsigned char c : value) {
        if (!std::isdigit(c)) return false;
    }
    return true;
}

bool isAlias(const std::string& token, const std::vector<std::string>& aliases) {
    const std::string candidate = upper(token);
    for (std::string alias : aliases) {
        alias = upper(alias);
        if (alias.empty() || candidate.rfind(alias, 0) != 0) continue;
        const size_t dash = candidate.find('-', alias.size() + 1);
        if (dash == std::string::npos) continue;
        if (isDecimal(candidate.substr(alias.size(), dash - alias.size())) &&
            isDecimal(candidate.substr(dash + 1))) return true;
    }
    return candidate == "RELAY" || candidate == "TRACE";
}

bool isInternetOnly(const std::string& token) {
    const std::string value = upper(token);
    return value == "TCPIP" || value == "TCPXX" || value == "RFONLY" ||
           value == "NOGATE" || (value.size() == 3 && value.rfind("QA", 0) == 0);
}

bool validRelay(const std::string& token) {
    if (token.empty()) return false;
    for (const unsigned char c : token) {
        if (std::isspace(c) || c == ',' || c == ':' || c == '*') return false;
    }
    return true;
}

} // namespace

namespace APRS_RETURN_ROUTE {

Result derive(const std::string& packet, const std::string& ownCallsign,
              const std::string& ownDigiCallsign,
              const std::vector<std::string>& regionalAliases) {
    const size_t gt = packet.find('>');
    const size_t colon = packet.find(':', gt == std::string::npos ? 0 : gt + 1);
    if (gt == std::string::npos || gt == 0 || colon == std::string::npos || colon <= gt + 1) {
        return {};
    }

    const std::string header = packet.substr(gt + 1, colon - gt - 1);
    const std::vector<std::string> fields = split(header, ',');
    if (fields.empty() || fields.front().empty()) return {};

    size_t lastUsed = std::string::npos;
    for (size_t i = 1; i < fields.size(); ++i) {
        if (!fields[i].empty() && fields[i].back() == '*') lastUsed = i;
    }

    Result result;
    result.valid = true;
    if (lastUsed == std::string::npos) return result;

    const std::string own = upper(ownCallsign);
    const std::string ownDigi = upper(ownDigiCallsign);
    std::vector<std::string> usedRelays;
    for (size_t i = 1; i <= lastUsed; ++i) {
        std::string token = fields[i];
        if (!token.empty() && token.back() == '*') token.pop_back();
        if (!validRelay(token)) return {};
        if (upper(token) == own || (!ownDigi.empty() && upper(token) == ownDigi) ||
            isInternetOnly(token) || isAlias(token, regionalAliases)) continue;
        for (const std::string& existing : usedRelays) {
            if (upper(existing) == upper(token)) return {};
        }
        usedRelays.push_back(token);
    }

    for (auto it = usedRelays.rbegin(); it != usedRelays.rend(); ++it) {
        if (!result.path.empty()) result.path += ',';
        result.path += *it;
    }
    return result;
}

std::string buildLocalReplyHeader(const std::string& source,
                                 const Result& route,
                                 const std::string& fallbackPath,
                                 bool rfOnly) {
    const std::string& path = route.valid ? route.path : fallbackPath;
    std::string header = source + ">APLRG1";
    if (!path.empty()) header += ',' + path;
    if (rfOnly) header += ",RFONLY";
    return header;
}

std::string buildThirdPartyMessage(const std::string& iGateCallsign,
                                   const std::string& path,
                                   const std::string& aprsisPacket) {
    if (iGateCallsign.empty()) return {};
    const size_t information = aprsisPacket.find(':');
    const size_t message = aprsisPacket.find("::", information);
    if (information == std::string::npos || message == std::string::npos) return {};

    const size_t comma = aprsisPacket.find(',');
    const size_t innerHeaderEnd = comma != std::string::npos && comma < information
                                    ? comma : information;
    if (innerHeaderEnd == 0 || aprsisPacket.find('>') >= innerHeaderEnd) return {};

    std::string output = iGateCallsign + ">APLRG1";
    if (!path.empty()) output += ',' + path;
    output += ":}" + aprsisPacket.substr(0, innerHeaderEnd);
    output += ",TCPIP," + iGateCallsign + '*';
    output += aprsisPacket.substr(message);
    return output;
}

} // namespace APRS_RETURN_ROUTE
