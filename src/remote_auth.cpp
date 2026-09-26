/* Copyright (C) 2026 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate.
 *
 * LoRa APRS iGate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <Preferences.h>
#include <mbedtls/md.h>
#include <cstring>
#include <mutex>
#include <vector>

#include "remote_auth.h"
#include "remote_replay_window.h"

namespace {

constexpr char kPrefix[] = "!RC1:";
constexpr char kDomain[] = "LORA-APRS-RC";
constexpr char kNamespace[] = "remote-auth";
constexpr char kSecretKey[] = "secret";
constexpr char kCounterKey[] = "rx-counter";
constexpr char kReplayWindowKey[] = "rx-window";
constexpr size_t kSecretLength = 32;
constexpr size_t kTagLength = 12;
std::mutex authMutex;

REMOTE_REPLAY::State loadReplayState() {
    Preferences preferences;
    if (!preferences.begin(kNamespace, true)) return {};
    REMOTE_REPLAY::State state;
    if (preferences.getBytesLength(kReplayWindowKey) == sizeof(state) &&
        preferences.getBytes(kReplayWindowKey, &state, sizeof(state)) == sizeof(state)) {
        preferences.end();
        return state;
    }

    // Migrate safely from the original single high-water counter. Every
    // earlier value remains blocked until it ages out of the new window.
    state.highest = preferences.getULong64(kCounterKey, 0);
    state.seen = state.highest == 0 ? 0 : UINT64_MAX;
    preferences.end();
    return state;
}

void secureZero(void* memory, size_t length) {
    volatile uint8_t* bytes = static_cast<volatile uint8_t*>(memory);
    while (length-- > 0) *bytes++ = 0;
}

int decodeBase64UrlChar(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
}

bool decodeBase64Url(const String& encoded, std::vector<uint8_t>& decoded) {
    if (encoded.length() == 0 || encoded.indexOf('=') >= 0 || encoded.length() % 4 == 1) return false;
    decoded.clear();
    uint32_t accumulator = 0;
    int bits = 0;
    for (size_t i = 0; i < encoded.length(); ++i) {
        const int value = decodeBase64UrlChar(encoded[i]);
        if (value < 0) return false;
        accumulator = (accumulator << 6) | static_cast<uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            decoded.push_back(static_cast<uint8_t>((accumulator >> bits) & 0xff));
        }
    }
    if (bits > 0 && (accumulator & ((1U << bits) - 1U)) != 0) return false;
    return true;
}

String encodeBase64Url(const uint8_t* data, size_t length) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    String encoded;
    encoded.reserve((length * 4 + 2) / 3);
    uint32_t accumulator = 0;
    int bits = 0;
    for (size_t i = 0; i < length; ++i) {
        accumulator = (accumulator << 8) | data[i];
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            encoded += alphabet[(accumulator >> bits) & 0x3f];
        }
    }
    if (bits > 0) encoded += alphabet[(accumulator << (6 - bits)) & 0x3f];
    return encoded;
}

void appendU16(std::vector<uint8_t>& bytes, size_t value) {
    bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    bytes.push_back(static_cast<uint8_t>(value & 0xff));
}

void appendU64(std::vector<uint8_t>& bytes, uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<uint8_t>((value >> shift) & 0xff));
    }
}

void appendString(std::vector<uint8_t>& bytes, const String& value) {
    appendU16(bytes, value.length());
    bytes.insert(bytes.end(), value.c_str(), value.c_str() + value.length());
}

bool parseBase36(const String& text, uint64_t& value) {
    if (text.length() == 0 || text.length() > 13 ||
        (text.length() > 1 && text[0] == '0')) return false;
    value = 0;
    for (size_t i = 0; i < text.length(); ++i) {
        uint8_t digit;
        const char c = text[i];
        if (c >= '0' && c <= '9') digit = static_cast<uint8_t>(c - '0');
        else if (c >= 'A' && c <= 'Z') digit = static_cast<uint8_t>(c - 'A' + 10);
        else return false;
        if (value > (UINT64_MAX - digit) / 36U) return false;
        value = value * 36U + digit;
    }
    return value != 0;
}

bool allowedCommand(const String& command) {
    return command == "EM=ON" || command == "EM=OFF" || command == "TX=ON" ||
           command == "TX=OFF" || command == "COMMIT";
}

bool loadSecret(uint8_t secret[kSecretLength]) {
    Preferences preferences;
    if (!preferences.begin(kNamespace, true)) return false;
    const bool present = preferences.getBytesLength(kSecretKey) == kSecretLength;
    const bool loaded = present && preferences.getBytes(kSecretKey, secret, kSecretLength) == kSecretLength;
    preferences.end();
    return loaded;
}

String calculateTag(const uint8_t secret[kSecretLength], const String& controller,
                    const String& target, char keyId, uint64_t counter, const String& command) {
    std::vector<uint8_t> authenticated;
    authenticated.reserve(sizeof(kDomain) - 1 + controller.length() + target.length() +
                          command.length() + 16);
    authenticated.insert(authenticated.end(), kDomain, kDomain + sizeof(kDomain) - 1);
    authenticated.push_back(1);
    appendString(authenticated, controller);
    appendString(authenticated, target);
    authenticated.push_back(static_cast<uint8_t>(keyId));
    appendU64(authenticated, counter);
    appendString(authenticated, command);

    uint8_t digest[32] = {};
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (info == nullptr || mbedtls_md_hmac(info, secret, kSecretLength, authenticated.data(),
                                           authenticated.size(), digest) != 0) {
        secureZero(digest, sizeof(digest));
        return "";
    }
    const String tag = encodeBase64Url(digest, kTagLength);
    secureZero(digest, sizeof(digest));
    return tag;
}

bool constantTimeEqual(const String& left, const String& right) {
    if (left.length() != right.length()) return false;
    uint8_t difference = 0;
    for (size_t i = 0; i < left.length(); ++i) {
        difference |= static_cast<uint8_t>(left[i]) ^ static_cast<uint8_t>(right[i]);
    }
    return difference == 0;
}

} // namespace

namespace REMOTE_AUTH {

bool configured() {
    Preferences preferences;
    if (!preferences.begin(kNamespace, true)) return false;
    const bool present = preferences.getBytesLength(kSecretKey) == kSecretLength;
    preferences.end();
    return present;
}

uint64_t acceptedCounter() {
    return loadReplayState().highest;
}

bool setSecret(const String& base64UrlSecret) {
    const std::lock_guard<std::mutex> lock(authMutex);
    std::vector<uint8_t> decoded;
    if (!decodeBase64Url(base64UrlSecret, decoded) || decoded.size() != kSecretLength) return false;
    uint8_t existing[kSecretLength];
    const bool unchanged = loadSecret(existing) &&
                           memcmp(existing, decoded.data(), kSecretLength) == 0;
    secureZero(existing, sizeof(existing));
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        secureZero(decoded.data(), decoded.size());
        return false;
    }
    const bool written = preferences.putBytes(kSecretKey, decoded.data(), decoded.size()) == decoded.size();
    const REMOTE_REPLAY::State emptyState;
    const bool reset = unchanged ||
                       (written &&
                        preferences.putBytes(kReplayWindowKey, &emptyState, sizeof(emptyState)) == sizeof(emptyState) &&
                        preferences.putULong64(kCounterKey, 0) == sizeof(uint64_t));
    preferences.end();
    secureZero(decoded.data(), decoded.size());
    return written && reset;
}

bool clearSecret() {
    const std::lock_guard<std::mutex> lock(authMutex);
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) return false;
    const bool secretRemoved = preferences.remove(kSecretKey) || preferences.getBytesLength(kSecretKey) == 0;
    const bool counterRemoved = preferences.remove(kCounterKey) || preferences.getULong64(kCounterKey, 0) == 0;
    const bool windowRemoved = preferences.remove(kReplayWindowKey) ||
                               preferences.getBytesLength(kReplayWindowKey) == 0;
    preferences.end();
    return secretRemoved && counterRemoved && windowRemoved;
}

Result verifyAndConsume(const String& envelope, const String& controller,
                        const String& expectedController, const String& target) {
    const std::lock_guard<std::mutex> lock(authMutex);
    Result result;
    if (!envelope.startsWith(kPrefix)) return result;
    if (!configured() || expectedController.length() == 0) {
        result.status = Status::Disabled;
        return result;
    }
    if (controller != expectedController) {
        result.status = Status::WrongController;
        return result;
    }

    const int keyEnd = envelope.indexOf(':', sizeof(kPrefix) - 1);
    const int counterEnd = keyEnd < 0 ? -1 : envelope.indexOf(':', keyEnd + 1);
    const int commandEnd = counterEnd < 0 ? -1 : envelope.indexOf(':', counterEnd + 1);
    if (keyEnd < 0 || counterEnd < 0 || commandEnd < 0 ||
        envelope.indexOf(':', commandEnd + 1) >= 0) {
        result.status = Status::Malformed;
        return result;
    }
    const String key = envelope.substring(sizeof(kPrefix) - 1, keyEnd);
    const String counterText = envelope.substring(keyEnd + 1, counterEnd);
    result.command = envelope.substring(counterEnd + 1, commandEnd);
    const String suppliedTag = envelope.substring(commandEnd + 1);
    if (key != "A" || suppliedTag.length() != 16 || !allowedCommand(result.command) ||
        !parseBase36(counterText, result.counter)) {
        result.status = Status::Malformed;
        return result;
    }
    uint8_t secret[kSecretLength];
    if (!loadSecret(secret)) {
        result.status = Status::Disabled;
        return result;
    }
    const String expectedTag = calculateTag(secret, controller, target, 'A', result.counter, result.command);
    secureZero(secret, sizeof(secret));
    if (expectedTag.length() == 0 || !constantTimeEqual(suppliedTag, expectedTag)) {
        result.status = Status::BadTag;
        return result;
    }

    REMOTE_REPLAY::State replayState = loadReplayState();
    if (!REMOTE_REPLAY::consume(replayState, result.counter)) {
        result.status = Status::Replay;
        return result;
    }

    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        result.status = Status::StorageError;
        return result;
    }
    const bool windowStored = preferences.putBytes(
        kReplayWindowKey, &replayState, sizeof(replayState)) == sizeof(replayState);
    if (!windowStored) {
        preferences.end();
        result.status = Status::StorageError;
        return result;
    }
    preferences.end();
    result.status = Status::Accepted;
    return result;
}

const char* statusText(Status status) {
    switch (status) {
        case Status::Disabled: return "disabled";
        case Status::Malformed: return "malformed";
        case Status::WrongController: return "wrong controller";
        case Status::Replay: return "replayed counter";
        case Status::BadTag: return "bad tag";
        case Status::StorageError: return "storage error";
        case Status::Accepted: return "accepted";
        default: return "not authenticated";
    }
}

} // namespace REMOTE_AUTH
