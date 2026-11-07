#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
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
// Network route key, installed locally with the station secret (43 Base64URL characters).
bool networkKeyConfigured();
bool setNetworkKey(const char* encodedKey);
bool clearNetworkKey();
// Route set by a signed RF command; nullptr on success, otherwise the reply text.
const char* setRadioRelay(const char* controller,const char* target);
const char* setRadioTarget(const char* controller,const char* relay);
const char* clearRadioRoute();
void describeRoute(char* out,size_t size);
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
inline bool networkKeyConfigured() {return false;}
inline bool setNetworkKey(const char*) {return false;}
inline bool clearNetworkKey() {return false;}
inline const char* setRadioRelay(const char*,const char*) {return "OTA ROUTE UNSUPPORTED";}
inline const char* setRadioTarget(const char*,const char*) {return "OTA ROUTE UNSUPPORTED";}
inline const char* clearRadioRoute() {return "OTA ROUTE UNSUPPORTED";}
inline void describeRoute(char* out,size_t size) {if(size)snprintf(out,size,"ROUTE UNSUPPORTED");}
inline void setupWeb(AsyncWebServer*) {}
#endif
}
