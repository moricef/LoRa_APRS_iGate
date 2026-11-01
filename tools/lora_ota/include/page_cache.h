#pragma once
#include "window_session.h"
namespace labota {
// Signed frames remain byte-identical. Payloads share one 4 KiB page buffer;
// keeping 128 full 255-byte frame slots would unnecessarily consume 32 KiB.
struct PageCache {
    uint32_t base=0;
    WindowBits bits;
    WindowGeometry geometry;
    uint8_t nonce[8]{},lengths[WindowGeometry::MAX_BLOCKS]{};
    uint8_t headers[WindowGeometry::MAX_BLOCKS][51]{},pageBytes[PAGE_BYTES]{};
    void clear() {bits=0;memset(lengths,0,sizeof lengths);}
    size_t frame(unsigned index,uint8_t* out) const {
        if(!bits.test(index))return 0;
        size_t n=lengths[index],payload=n-51;
        memcpy(out,headers[index],35);memcpy(out+35,pageBytes+index*geometry.chunk,payload);
        memcpy(out+35+payload,headers[index]+35,16);return n;
    }
    bool put(const uint8_t* f,size_t n) {
        if(n<52 || n>size_t(51)+geometry.chunk || n>255 || memcmp(f,"\x3c\xff\x7e\x02",4) || f[4]!=WINDOW_DATA ||
           size_t(f[17])*256+f[18]!=n-51)return false;
        uint32_t offset=get32(f+13),inPage=offset%PAGE_BYTES;
        if(inPage%geometry.chunk || n-51>std::min(uint32_t(geometry.chunk),PAGE_BYTES-inPage))return false;
        uint32_t page=offset/PAGE_BYTES*PAGE_BYTES,index=(offset-page)/geometry.chunk;
        if(!bits || page!=base || memcmp(nonce,f+5,8)) {clear();base=page;memcpy(nonce,f+5,8);}
        if(bits.test(index))return lengths[index]==n && !memcmp(headers[index],f,35) &&
            !memcmp(headers[index]+35,f+n-16,16) && !memcmp(pageBytes+inPage,f+35,n-51);
        memcpy(headers[index],f,35);memcpy(headers[index]+35,f+n-16,16);
        memcpy(pageBytes+inPage,f+35,n-51);lengths[index]=uint8_t(n);bits.set(index);return true;
    }
    WindowBits available(uint32_t page,const uint8_t* session) const {
        return page==base && !memcmp(nonce,session,8)?bits:WindowBits();
    }
};
}
