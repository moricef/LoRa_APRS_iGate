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
inline bool named(const uint8_t* field,const char* station) {
    uint8_t b[8]{};size_t n=strlen(station);if(!n||n>8)return false;memcpy(b,station,n);return !memcmp(b,field,8);
}
// A relay stands between two neighbours. Downstream frames are accepted only
// from the previous hop (the controller itself, or the relay before this one)
// and upstream frames only from the next hop (the target itself, or the relay
// after this one). Every relay re-emits with its own name as hop source, so a
// frame overheard from any other station of the chain is ignored.
inline bool downstreamFrom(const View& v,const char* controller,const char* previous) {
    return v.phase==(strcmp(previous,controller)?RELAY_TARGET:CONTROLLER_RELAY) && named(v.hopSource,previous);
}
inline bool upstreamFrom(const View& v,const char* target,const char* next) {
    return v.phase==(strcmp(next,target)?RELAY_CONTROLLER:TARGET_RELAY) && named(v.hopSource,next);
}
// Neighbours of a relay must be two distinct stations, neither being itself;
// the controller can only be upstream and the target only downstream.
inline bool validNeighbours(const char* self,const char* controller,const char* target,
        const char* previous,const char* next) {
    return strcmp(previous,self) && strcmp(next,self) && strcmp(previous,next) &&
        strcmp(previous,target) && strcmp(next,controller);
}
inline size_t envelope(uint8_t phase,const uint8_t* hopSource,const uint8_t* inner,size_t size,uint8_t* output) {
    if(phase>3 || size<51 || size>MAX_INNER)return 0;
    memcpy(output,"\x3c\xff\x7d\x01",4);output[4]=phase;
    memcpy(output+5,hopSource,8);memcpy(output+13,inner,size);return 13+size; // caller appends the 16-byte route tag
}
}
