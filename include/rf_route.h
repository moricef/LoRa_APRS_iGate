#pragma once
#include <cstddef>
#include <cstdint>
class AsyncWebServer;
namespace RF_ROUTE {
#ifdef ENABLE_RF_ROUTE
void recoverConfiguration();
void begin();
void tick();
bool busy();
bool handle(const uint8_t*,size_t);
bool requiresRelay();
bool routedReply();
size_t wrapReply(const uint8_t*,size_t,uint8_t*);
bool configureTarget(const char* controller,const char* relay,const char* encodedKey);
void setupWeb(AsyncWebServer*);
#else
inline void recoverConfiguration() {}
inline void begin() {}
inline void tick() {}
inline bool busy() {return false;}
inline bool handle(const uint8_t*,size_t) {return false;}
inline bool requiresRelay() {return false;}
inline bool routedReply() {return false;}
inline size_t wrapReply(const uint8_t*,size_t,uint8_t*) {return 0;}
inline bool configureTarget(const char*,const char*,const char*) {return false;}
inline void setupWeb(AsyncWebServer*) {}
#endif
}
