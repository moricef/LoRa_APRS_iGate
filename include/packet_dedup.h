#ifndef PACKET_DEDUP_H_
#define PACKET_DEDUP_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PACKET_DEDUP {

constexpr uint8_t DIGI = 1U << 0;
constexpr uint8_t APRSIS = 1U << 1;
constexpr uint32_t DEFAULT_WINDOW_MS = 25000;
constexpr size_t DEFAULT_MAX_ENTRIES = 256;

struct Entry {
    uint64_t fingerprint;
    uint32_t firstSeenMs;
    uint8_t destinations;
};

uint64_t fingerprint(const std::string& source, const std::string& information);

class Cache {
public:
    explicit Cache(uint32_t windowMs = DEFAULT_WINDOW_MS,
                   size_t maxEntries = DEFAULT_MAX_ENTRIES);

    // Returns true only when this source+information pair has not yet been
    // claimed for the requested destination during the active time window.
    // The destination bit is set before the function returns.
    bool claim(const std::string& source, const std::string& information,
               uint8_t destination, uint32_t nowMs);

    size_t size() const { return entries_.size(); }

private:
    void expire(uint32_t nowMs);

    uint32_t windowMs_;
    size_t maxEntries_;
    std::vector<Entry> entries_;
};

} // namespace PACKET_DEDUP

#endif
