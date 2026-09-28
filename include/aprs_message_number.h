#ifndef APRS_MESSAGE_NUMBER_H_
#define APRS_MESSAGE_NUMBER_H_

#include <string>

namespace APRS_MESSAGE {

inline bool alphanumeric(const std::string& value) {
    for (unsigned char c : value) {
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z'))) return false;
    }
    return true;
}

// APRS 1.1 replyacks.txt, step 4: echo the complete MM}AA in the ACK.
// MM} advertises reply-ack support without an outstanding piggybacked ACK.
inline std::string ackText(const std::string& number) {
    if (number.empty() || number.size() > 5) return {};
    const size_t separator = number.find('}');
    if (separator == std::string::npos) {
        if (!alphanumeric(number)) return {};
    } else {
        const std::string own = number.substr(0, separator);
        const std::string reply = number.substr(separator + 1);
        if (own.empty() || own.size() > 2 || reply.size() > 2 ||
            !alphanumeric(own) || !alphanumeric(reply)) return {};
    }
    return "ack" + number;
}

inline std::string dedupKey(const std::string& message) {
    const size_t brace = message.find('{');
    if (brace == std::string::npos) return message;
    const std::string number = message.substr(brace + 1);
    if (ackText(number).empty()) return message;
    const size_t separator = number.find('}');
    // AA may change on a retry of the same MM (replyacks.txt, step 3).
    return separator == std::string::npos
        ? message : message.substr(0, brace + 1 + separator);
}

} // namespace APRS_MESSAGE

#endif
