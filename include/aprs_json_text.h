#pragma once

#include <cstddef>
#include <cstdint>

namespace APRS_JSON_Text {

bool isSafeUtf8(const uint8_t* bytes, size_t length);

}  // namespace APRS_JSON_Text
