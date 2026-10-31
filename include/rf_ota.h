#pragma once
#include <cstddef>
#include <cstdint>
#include <Arduino.h>
namespace RF_OTA {
#ifdef ENABLE_RF_OTA
void earlyBoot();
void begin();
void tick();
bool handle(const uint8_t* data,size_t size);
bool busy();
bool handleAprsControl(const String& packet);
#else
inline void earlyBoot() {}
inline void begin() {}
inline void tick() {}
inline bool handle(const uint8_t*,size_t) { return false; }
inline bool busy() { return false; }
inline bool handleAprsControl(const String&) { return false; }
#endif
}
