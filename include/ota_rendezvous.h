#pragma once
#include "ota_radio_profile.h"
#include <string>

namespace labota {
inline bool rendezvousControl(const std::string& input,const std::string& controller,
        const std::string& recipient,const std::string& local,std::string& output) {
    std::string packet=input;
    if(packet.compare(0,3,"\x3c\xff\x01")==0)packet.erase(0,3);
    const auto gt=packet.find('>'),colon=packet.find(':');
    if(gt==std::string::npos || !gt || colon==std::string::npos || colon<gt ||
       packet.size()<colon+12 || packet[colon+1]!=':' || packet[colon+11]!=':')return false;
    const auto from=packet.substr(0,gt);auto to=packet.substr(colon+2,9);
    while(!to.empty() && to.back()==' ')to.pop_back();
    const auto text=packet.substr(colon+12);
    bool outbound=from==controller && to==recipient && text.compare(0,5,"!RC1:")==0 &&
        text.find(":OTA=")!=std::string::npos;
    bool inbound=from==recipient && to==controller &&
        (text.compare(0,4,"OTA ")==0 || text.compare(0,3,"ack")==0 || text=="AUTH FAILED");
    if(!outbound && !inbound)return false;
    for(size_t token=gt+1;token<colon;) {
        auto end=packet.find(',',token);if(end==std::string::npos || end>colon)end=colon;
        if(packet.substr(token,end-token)==local) {
            packet.insert(end,"*");output="\x3c\xff\x01"+packet;return output.size()<=255;
        }
        token=end+1;
    }
    return false;
}
inline void encodeCoordinationState(uint8_t* out,uint8_t phase,bool armed,bool routeBusy,
        bool known,const RadioProfile& service,const RadioProfile& applied,uint32_t sequence) {
    out[0]=1;out[1]=phase;out[2]=armed;out[3]=routeBusy;out[4]=known;
    service.encode(out+5);applied.encode(out+16);RadioProfile::write32(out+27,sequence);
}
// Only authenticated, strictly increasing keepalives renew this outer lease.
// The inner transfer can return here without returning to the station service.
struct Rendezvous {
    enum Phase : uint8_t { SERVICE, WAITING, CONTROL, RETURNING } phase=SERVICE;
    enum Event { NONE, ENTER, RESTORE };
    RadioProfile radio;
    uint32_t deadline=0,started=0,lastActivity=0,sequence=0;
    bool prepare(uint32_t now,const RadioProfile& selected) {
        if(phase!=SERVICE)return false;
        radio=selected;phase=WAITING;deadline=now+2000;started=now;sequence=0;
        return true;
    }
    bool keep(uint32_t now,uint32_t next) {
        if(phase!=CONTROL || !next || next<=sequence || expired(now))return false;
        sequence=next;lastActivity=now;return true;
    }
    bool expired(uint32_t now) const {
        return uint32_t(now-started)>=7200000U ||
            (phase==CONTROL && uint32_t(now-lastActivity)>=60000U);
    }
    void leave(uint32_t now) {
        if(phase==SERVICE || phase==RETURNING)return;
        phase=RETURNING;deadline=now+2000;
    }
    Event poll(uint32_t now) {
        if(phase==SERVICE)return NONE;
        if(expired(now) || (phase==RETURNING && int32_t(now-deadline)>=0)) {
            phase=SERVICE;return RESTORE;
        }
        if(phase==WAITING && int32_t(now-deadline)>=0) {
            phase=CONTROL;lastActivity=now;return ENTER;
        }
        return NONE;
    }
    void reset() {phase=SERVICE;sequence=0;}
};
}
