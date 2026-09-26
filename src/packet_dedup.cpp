#include "packet_dedup.h"

#include "rxt_protocol.h"

namespace {

constexpr uint64_t kFnvOffset = UINT64_C(14695981039346656037);
constexpr uint64_t kFnvPrime = UINT64_C(1099511628211);

void addByte(uint64_t& hash, uint8_t value) {
    hash ^= value;
    hash *= kFnvPrime;
}

} // namespace

namespace PACKET_DEDUP {

uint64_t fingerprint(const std::string& source, const std::string& information) {
    const std::string cleanInformation = RXT_Protocol::stripTrailer(information);
    uint64_t hash = kFnvOffset;

    // Length-prefix the source so concatenation boundaries are unambiguous.
    const uint16_t sourceLength = static_cast<uint16_t>(source.size());
    addByte(hash, static_cast<uint8_t>(sourceLength >> 8));
    addByte(hash, static_cast<uint8_t>(sourceLength));
    for (const unsigned char value : source) addByte(hash, value);
    for (const unsigned char value : cleanInformation) addByte(hash, value);
    return hash;
}

Cache::Cache(uint32_t windowMs, size_t maxEntries)
    : windowMs_(windowMs), maxEntries_(maxEntries) {}

void Cache::expire(uint32_t nowMs) {
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (static_cast<uint32_t>(nowMs - it->firstSeenMs) > windowMs_) {
            it = entries_.erase(it);
        } else {
            ++it;
        }
    }
}

bool Cache::claim(const std::string& source, const std::string& information,
                  uint8_t destination, uint32_t nowMs) {
    if (destination == 0) return false;
    expire(nowMs);
    const uint64_t value = fingerprint(source, information);
    for (Entry& entry : entries_) {
        if (entry.fingerprint != value) continue;
        if ((entry.destinations & destination) != 0) return false;
        entry.destinations |= destination;
        return true;
    }

    if (maxEntries_ == 0) return false;
    if (entries_.size() >= maxEntries_) entries_.erase(entries_.begin());
    entries_.push_back({value, nowMs, destination});
    return true;
}

} // namespace PACKET_DEDUP
