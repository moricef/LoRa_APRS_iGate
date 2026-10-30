#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
namespace routeota {
constexpr size_t OVERHEAD=29,MAX_INNER=226,CHUNK=175;
enum Phase:uint8_t { CONTROLLER_RELAY=0,RELAY_TARGET=1,TARGET_RELAY=2,RELAY_CONTROLLER=3 };
struct View {uint8_t phase;const uint8_t* hopSource;const uint8_t* inner;size_t size;};
inline bool parse(const uint8_t* f,size_t n,View& v) {
    if(n<80 || n>255 || memcmp(f,"\x3c\xff\x7d\x01",4) || f[4]>3)return false;
    v={f[4],f+5,f+13,n-OVERHEAD};
    if(memcmp(v.inner,"\x3c\xff\x7e\x02",4))return false;
    size_t body=(size_t(v.inner[17])<<8)|v.inner[18];
    return v.size==51+body;
}
inline size_t envelope(uint8_t phase,const uint8_t* hopSource,const uint8_t* inner,size_t size,uint8_t* output) {
    if(phase>3 || size<51 || size>MAX_INNER)return 0;
    memcpy(output,"\x3c\xff\x7d\x01",4);output[4]=phase;
    memcpy(output+5,hopSource,8);memcpy(output+13,inner,size);return 13+size; // caller appends the 16-byte route tag
}
}
