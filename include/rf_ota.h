#pragma once
#include <cstddef>
#include <cstdint>
namespace RF_OTA {
#ifdef ENABLE_RF_OTA
void earlyBoot();
void begin();
void tick();
bool handle(const uint8_t* data,size_t size);
bool busy();
#else
inline void earlyBoot() {}
inline void begin() {}
inline void tick() {}
inline bool handle(const uint8_t*,size_t) { return false; }
inline bool busy() { return false; }
#endif
}
