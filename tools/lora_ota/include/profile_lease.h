#pragma once
#include <cstdint>
namespace labota {
// Volatile lease: a reset always reloads the configured normal radio profile.
struct ProfileLease {
    enum Mode { NORMAL, WAIT_FAST, FAST, WAIT_NORMAL } mode=NORMAL;
    enum Event { NONE, ENTER_FAST, RESTORE_NORMAL };
    uint32_t deadline=0,lastActivity=0;
    uint16_t seconds=0;
    bool prepare(uint32_t now,uint16_t lease) {
        if(lease<15 || lease>60)return false;
        if(mode==WAIT_FAST)return seconds==lease; // ACK retry must not postpone switching
        if(mode!=NORMAL)return false;
        seconds=lease;deadline=now+2000;mode=WAIT_FAST;return true;
    }
    void requestReturn(uint32_t now) {
        if(mode==NORMAL || mode==WAIT_NORMAL)return;
        deadline=now+500;mode=WAIT_NORMAL;
    }
    void activity(uint32_t now) {if(mode==FAST)lastActivity=now;}
    Event poll(uint32_t now) {
        if(mode==WAIT_FAST && int32_t(now-deadline)>=0) {
            mode=FAST;lastActivity=now;return ENTER_FAST;
        }
        if((mode==WAIT_NORMAL && int32_t(now-deadline)>=0) ||
           (mode==FAST && uint32_t(now-lastActivity)>=uint32_t(seconds)*1000)) {
            mode=NORMAL;return RESTORE_NORMAL;
        }
        return NONE;
    }
    void reset() {mode=NORMAL;seconds=0;}
};
}
