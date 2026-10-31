#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace routeota {
constexpr uint8_t TEST_RELAY_RESTART=21;
constexpr uint8_t TEST_RELAY_FAULT=22;
// Call only after validating the route MAC, phase and both endpoint identities.
// A captured request from another session must not reboot an active relay.
inline bool restartMatchesSession(const uint8_t* inner,size_t size,bool active,const uint8_t* nonce) {
    return active && size==51 && inner[4]==TEST_RELAY_RESTART &&
        !memcmp(inner+5,nonce,8) && inner[13]==0 && inner[14]==0 &&
        inner[15]==0 && inner[16]==0 && inner[17]==0 && inner[18]==0;
}
struct RelayFault {
    uint8_t mode=0,nonce[8]{};
    uint32_t base=0,forwarded=0;
    void clear() {mode=0;forwarded=0;}
    bool arm(const uint8_t* inner,size_t size,bool active,const uint8_t* sessionNonce) {
        if(!active || size!=52 || inner[4]!=TEST_RELAY_FAULT ||
           inner[17]!=0 || inner[18]!=1 || (inner[35]!=1 && inner[35]!=2) ||
           memcmp(inner+5,sessionNonce,8))return false;
        uint32_t offset=(uint32_t(inner[13])<<24)|(uint32_t(inner[14])<<16)|
                        (uint32_t(inner[15])<<8)|inner[16];
        if(offset%4096)return false;
        mode=inner[35];base=offset;forwarded=0;memcpy(nonce,sessionNonce,8);return true;
    }
    bool afterBlock(uint32_t page,const uint8_t* sessionNonce) {
        return mode==1 && page==base && !memcmp(nonce,sessionNonce,8) && ++forwarded==16;
    }
    bool afterBitmap(uint32_t page,const uint8_t* sessionNonce,uint32_t written,uint32_t bitmap) const {
        return mode==2 && page==base && !memcmp(nonce,sessionNonce,8) &&
               written==base+4096 && bitmap==0xffffffffU;
    }
};
}
