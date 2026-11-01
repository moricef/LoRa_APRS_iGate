#pragma once
#include "session.h"
#include <algorithm>
namespace labota {
// Bits are block indices, low index first. Wire words are big-endian so the
// complete bitmap is also a big-endian integer for the Python controller.
struct WindowBits {
    uint32_t words[4]{};
    WindowBits(uint32_t low=0) {words[0]=low;}
    explicit operator bool() const {return words[0]||words[1]||words[2]||words[3];}
    bool operator==(const WindowBits& b) const {return !memcmp(words,b.words,sizeof words);}
    bool operator!=(const WindowBits& b) const {return !(*this==b);}
    WindowBits operator~() const {WindowBits b;for(int i=0;i<4;++i)b.words[i]=~words[i];return b;}
    WindowBits operator&(const WindowBits& b) const {WindowBits r;for(int i=0;i<4;++i)r.words[i]=words[i]&b.words[i];return r;}
    WindowBits& operator|=(const WindowBits& b) {for(int i=0;i<4;++i)words[i]|=b.words[i];return *this;}
    void set(unsigned i) {if(i<128)words[i/32]|=uint32_t(1)<<(i%32);}
    bool test(unsigned i) const {return i<128 && (words[i/32]&(uint32_t(1)<<(i%32)));}
    unsigned popFirst() {for(unsigned w=0;w<4;++w)if(words[w]) {unsigned b=__builtin_ctz(words[w]);words[w]&=~(uint32_t(1)<<b);return w*32+b;}return 128;}
    unsigned count() const {unsigned n=0;for(auto w:words)n+=__builtin_popcount(w);return n;}
    void encode(uint8_t* p,unsigned bytes) const {for(unsigned i=0;i<bytes/4;++i)put32(p+4*i,words[bytes/4-1-i]);}
    static WindowBits decode(const uint8_t* p,unsigned bytes) {WindowBits r;if(!bytes||bytes>16||bytes%4)return r;for(unsigned i=0;i<bytes/4;++i)r.words[bytes/4-1-i]=get32(p+4*i);return r;}
};
struct WindowGeometry {
    static constexpr unsigned PAGE=4096,MIN_CHUNK=32,MAX_CHUNK=204,MAX_BLOCKS=128;
    uint16_t chunk=128;
    bool configure(unsigned n) {if(n<MIN_CHUNK||n>MAX_CHUNK)return false;chunk=n;return true;}
    unsigned count(unsigned bytes=PAGE) const {return (bytes+chunk-1)/chunk;}
    unsigned bitmapBytes() const {return 4*((count()+31)/32);}
    WindowBits mask(unsigned bytes) const {WindowBits r;for(unsigned i=0;i<count(bytes);++i)r.set(i);return r;}
    static bool fromManifest(const uint8_t* p,size_t n,WindowGeometry& g) {
        if(n!=40&&n!=42)return false;
        return g.configure(n==40?128:(unsigned(p[40])<<8)|p[41]);
    }
};
}
