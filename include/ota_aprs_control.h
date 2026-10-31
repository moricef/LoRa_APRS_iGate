#pragma once
#include "ota_radio_profile.h"
#include <cstring>
#include <string>

namespace labota {
// APRS command arguments are decimal Hz, decimal BW Hz, SF, CR denominator,
// and requested transceiver dBm. No floating-point or partial parsing.
struct AprsOtaCommand {
    enum Kind { STATUS, PROFILE, FAST, NORMAL } kind=STATUS;
    RadioProfile radio;
    uint16_t lease=0;
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
