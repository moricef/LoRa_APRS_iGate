#ifndef LORA_FRAME_TYPE_H_
#define LORA_FRAME_TYPE_H_

#include <cstddef>
#include <cstdint>

namespace LORA_FRAME {

// Call after the supported binary decoders. A reserved LoRa APRS prefix
// must never fall through to the legacy text parser with an unknown type.
// Unprefixed TNC2 remains accepted for legacy transmitters.
inline bool mayReadAsText(const uint8_t* data, size_t size) {
    if (size < 2 || data[0] != 0x3c || data[1] != 0xff) return true;
    return size >= 3 && data[2] == 0x01;
}

}
#endif
