#ifndef LOCAL_MESSAGE_GATE_H_
#define LOCAL_MESSAGE_GATE_H_

#include "packet_dedup.h"
#include "aprs_message_number.h"

// One RF reception can reach both the iGate and digipeater consumers.
// ACK it once per reception, but execute an identical local query only once
// per duplicate window. A later RF retry can thus recover a lost ACK.
class LocalMessageGate {
public:
    struct Decision {
        bool acknowledge;
        bool executeQuery;
    };

    void beginReception() { handled_ = false; }

    Decision claim(const std::string& sender, const std::string& message,
                   bool isQuery, uint32_t nowMs) {
        if (handled_) return {false, false};
        handled_ = true;
        // Include the addressee, body and message number, but no RF path.
        // This cache is independent of relaying and return-route learning.
        return {true, isQuery && queries_.claim(
            sender, APRS_MESSAGE::dedupKey(message), 1U, nowMs)};
    }

private:
    bool handled_ = false;
    PACKET_DEDUP::Cache queries_;
};

#endif
