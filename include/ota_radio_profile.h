#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace labota {
enum class RadioChip { SX1278, SX1276, SX1262, SX1268, LLCC68 };
struct RadioProfile {
    uint32_t frequencyHz=434500000,bandwidthHz=500000;
    uint8_t sf=7,cr=5;
    int8_t power=2;
    constexpr RadioProfile(uint32_t frequency=434500000,uint32_t bandwidth=500000,uint8_t spreading=7,
                           uint8_t coding=5,int8_t dbm=2):
        frequencyHz(frequency),bandwidthHz(bandwidth),sf(spreading),cr(coding),power(dbm) {}
    bool operator==(const RadioProfile& b) const {
        return frequencyHz==b.frequencyHz && bandwidthHz==b.bandwidthHz && sf==b.sf && cr==b.cr && power==b.power;
    }
    bool valid(RadioChip chip) const {
        bool sx127=chip==RadioChip::SX1278 || chip==RadioChip::SX1276;
        uint32_t lo=sx127?137000000U:chip==RadioChip::SX1268?410000000U:150000000U;
        uint32_t hi=chip==RadioChip::SX1278?525000000U:chip==RadioChip::SX1276?1020000000U:
                    chip==RadioChip::SX1268?810000000U:960000000U;
        if(frequencyHz<lo || frequencyHz>hi || sf<(sx127?7:5) || sf>12 || cr<5 || cr>8)return false;
        // SX127x SF6 requires implicit headers and is excluded from this transport.
        bool bw=false;
        for(uint32_t allowed:{7800U,10400U,15600U,20800U,31250U,41700U,62500U,125000U,250000U,500000U})
            if(bandwidthHz==allowed)bw=true;
        if(!bw)return false;
        if(chip==RadioChip::LLCC68 && (bandwidthHz<125000U ||
            sf>(bandwidthHz==125000U?9:bandwidthHz==250000U?10:11)))return false;
        return sx127?((power>=2 && power<=17)||power==20):(power>=-9 && power<=22);
    }
    static uint32_t read32(const uint8_t* p) {
        return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];
    }
    static void write32(uint8_t* p,uint32_t n) {for(int i=3;i>=0;i--){p[i]=uint8_t(n);n>>=8;}}
    void encode(uint8_t* p) const {
        write32(p,frequencyHz);write32(p+4,bandwidthHz);p[8]=sf;p[9]=cr;p[10]=uint8_t(power);
    }
    static RadioProfile decode(const uint8_t* p) {
        return {read32(p),read32(p+4),p[8],p[9],int8_t(p[10])};
    }
};
struct FastProfileRequest {
    uint16_t lease=0;
    bool custom=false;
    RadioProfile radio;
    bool operator==(const FastProfileRequest& b) const {return lease==b.lease && custom==b.custom && radio==b.radio;}
    static bool decode(const uint8_t* p,size_t size,FastProfileRequest& out) {
        if(!((size==3 && p[0]==1)||(size==14 && p[0]==2)))return false;
        FastProfileRequest r;r.lease=(uint16_t(p[1])<<8)|p[2];r.custom=p[0]==2;
        if(r.lease<15 || r.lease>60)return false;
        if(r.custom)r.radio=RadioProfile::decode(p+3);
        out=r;return true;
    }
};
}
