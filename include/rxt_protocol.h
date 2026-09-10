#ifndef RXT_PROTOCOL_H_
#define RXT_PROTOCOL_H_

#include <string>
#include <vector>

namespace RXT_Protocol {

    std::string attachTrailer(const std::string& packet, const std::string& newTuple);
    std::string stripTrailer(const std::string& packet, std::string* outTuples = nullptr);
    std::vector<std::string> usedPathNodes(const std::string& packet);

}

#endif
