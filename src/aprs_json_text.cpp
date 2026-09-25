/* Copyright (C) 2026 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate and is distributed under GPLv3.
 */

#include "aprs_json_text.h"

namespace APRS_JSON_Text {

bool isSafeUtf8(const uint8_t* bytes, size_t length) {
    size_t i = 0;
    while (i < length) {
        uint8_t c = bytes[i++];
        if (c <= 0x7f) {
            if (c < 0x20 || c == 0x7f) return false;
            continue;
        }

        size_t remaining = 0;
        uint32_t codepoint = 0;
        if ((c & 0xe0) == 0xc0) {
            remaining = 1;
            codepoint = c & 0x1f;
            if (codepoint < 2) return false;
        } else if ((c & 0xf0) == 0xe0) {
            remaining = 2;
            codepoint = c & 0x0f;
        } else if ((c & 0xf8) == 0xf0) {
            remaining = 3;
            codepoint = c & 0x07;
        } else {
            return false;
        }
        if (i + remaining > length) return false;
        while (remaining--) {
            uint8_t continuation = bytes[i++];
            if ((continuation & 0xc0) != 0x80) return false;
            codepoint = (codepoint << 6) | (continuation & 0x3f);
        }
        if ((codepoint >= 0xd800 && codepoint <= 0xdfff) || codepoint > 0x10ffff) return false;
        if ((codepoint <= 0x7ff && (c & 0xf0) == 0xe0) ||
            (codepoint <= 0xffff && (c & 0xf8) == 0xf0)) return false;
    }
    return true;
}

}  // namespace APRS_JSON_Text
