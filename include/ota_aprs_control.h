#pragma once
#include "ota_radio_profile.h"
#include <cstring>
#include <string>

namespace labota {
// APRS command arguments are decimal Hz, decimal BW Hz, SF, CR denominator,
// and requested transceiver dBm. No floating-point or partial parsing.
// Route commands name one station: OTA=RELAY,<target> makes this station the
// relay of the authenticated controller towards <target>; OTA=VIA,<relay>
// makes it a target reached through <relay>. In a chain of relays,
// OTA=RELAY,<target>,<previous>,<next> names the neighbours of this relay:
// <previous> is the controller or the relay before it, <next> the relay after
// it or the target. The transport key is the network route key installed with
// the station secret, never sent over the air.
struct AprsOtaCommand {
    enum Kind { STATUS, PROFILE, FAST, NORMAL, ROUTE_STATUS, ROUTE_OFF, RELAY, VIA } kind=STATUS;
    RadioProfile radio;
    uint16_t lease=0;
    std::string peer,previous,next; // previous/next empty: controller and target
    // Route identities are at most eight characters (callsign-SSID or tactical).
    static bool station(const std::string& text) {
        if(text.empty()||text.size()>8)return false;
        for(char ch:text)
            if(!((ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='-'))return false;
        return text.front()!='-' && text.back()!='-';
    }
    // header is the TNC2 text between '>' and ':' (destination, then path).
    // True when the path names a station that has not repeated the packet yet:
    // that relay will transmit it again right now, so an immediate answer from
    // a station that also heard the original would collide with the repeat.
    static bool awaitsRelay(const std::string& header) {
        size_t start=header.find(',');
        while(start!=std::string::npos) {
            const size_t end=header.find(',',start+1);
            const std::string element=header.substr(start+1,end==std::string::npos?std::string::npos:end-start-1);
            start=end;
            if(element.empty() || element.back()=='*')continue;
            if(element=="RFONLY"||element=="NOGATE"||element=="TCPIP"||element=="TCPXX"||
               element.compare(0,4,"WIDE")==0||element.compare(0,5,"TRACE")==0)continue;
            if(station(element))return true;
        }
        return false;
    }
    static bool number(const std::string& text,int64_t& result) {
        if(text.empty())return false;
        size_t i=text[0]=='-'?1:0;if(i==text.size())return false;
        uint64_t value=0;
        for(;i<text.size();++i) {
            if(text[i]<'0'||text[i]>'9')return false;
            value=value*10+uint8_t(text[i]-'0');if(value>UINT32_MAX)return false;
        }
        result=text[0]=='-'?-int64_t(value):int64_t(value);return true;
    }
    static bool parse(const std::string& text,AprsOtaCommand& out) {
        AprsOtaCommand c;
        if(text=="OTA=?")c.kind=STATUS;
        else if(text=="OTA=NORMAL")c.kind=NORMAL;
        else if(text=="OTA=ROUTE?")c.kind=ROUTE_STATUS;
        else if(text=="OTA=ROUTE,OFF")c.kind=ROUTE_OFF;
        else if(text.compare(0,10,"OTA=RELAY,")==0) {
            const std::string rest=text.substr(10);
            const size_t first=rest.find(',');
            if(first==std::string::npos)c.peer=rest;
            else {
                const size_t second=rest.find(',',first+1);
                if(second==std::string::npos || rest.find(',',second+1)!=std::string::npos)return false;
                c.peer=rest.substr(0,first);c.previous=rest.substr(first+1,second-first-1);c.next=rest.substr(second+1);
                if(!station(c.previous)||!station(c.next))return false;
            }
            if(!station(c.peer))return false;
            c.kind=RELAY;
        } else if(text.compare(0,8,"OTA=VIA,")==0) {
            c.peer=text.substr(8);if(!station(c.peer))return false;c.kind=VIA;
        }
        else if(text.compare(0,9,"OTA=FAST,")==0) {
            int64_t value;if(!number(text.substr(9),value)||value<15||value>60)return false;
            c.kind=FAST;c.lease=uint16_t(value);
        } else if(text.compare(0,12,"OTA=PROFILE,")==0) {
            int64_t values[5];size_t pos=12;
            for(size_t i=0;i<5;++i) {
                size_t end=text.find(',',pos);
                if((i<4)==(end==std::string::npos))return false;
                if(!number(text.substr(pos,end==std::string::npos?end:end-pos),values[i]))return false;
                pos=end==std::string::npos?text.size():end+1;
            }
            if(values[0]<1||values[1]<1||values[2]<0||values[2]>255||
               values[3]<0||values[3]>255||values[4]<-128||values[4]>127)return false;
            c.kind=PROFILE;c.radio=RadioProfile(uint32_t(values[0]),uint32_t(values[1]),
                uint8_t(values[2]),uint8_t(values[3]),int8_t(values[4]));
        } else return false;
        out=c;return true;
    }
};
}
