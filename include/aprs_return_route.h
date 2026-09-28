#ifndef APRS_RETURN_ROUTE_H_
#define APRS_RETURN_ROUTE_H_

#include <string>
#include <vector>

namespace APRS_RETURN_ROUTE {

struct Result {
    bool valid = false;
    std::string path;
};

// Derive an explicit return path from the used portion of an RF TNC2 path.
// A valid empty path means that the source was heard directly.
Result derive(const std::string& packet, const std::string& ownCallsign,
              const std::string& ownDigiCallsign,
              const std::vector<std::string>& regionalAliases);

// A known empty route is DIRECT, not a reason to use the fallback path.
// RFONLY follows explicit relays so it cannot obstruct the next hop.
std::string buildLocalReplyHeader(const std::string& source,
                                 const Result& route,
                                 const std::string& fallbackPath,
                                 bool rfOnly);

// Wrap an APRS-IS message in a third-party RF frame using an explicit learned
// path. Returns an empty string when the input is not a valid APRS message.
std::string buildThirdPartyMessage(const std::string& iGateCallsign,
                                   const std::string& path,
                                   const std::string& aprsisPacket);

} // namespace APRS_RETURN_ROUTE

#endif
