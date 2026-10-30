#pragma once
#include "window_session.h"
namespace labota {
// Cache signed end-to-end frames unchanged; a relay never knows the target key.
struct PageCache {
    uint32_t base=0,bits=0;
    uint8_t nonce[8]{},lengths[WINDOW_BLOCKS]{};
    uint8_t frames[WINDOW_BLOCKS][51+WINDOW_CHUNK]{};
    void clear() {bits=0;memset(lengths,0,sizeof lengths);}
    bool put(const uint8_t* f,size_t n) {
        if(n<52 || n>51+WINDOW_CHUNK || memcmp(f,"\x3c\xff\x7e\x02",4) || f[4]!=WINDOW_DATA ||
           size_t(f[17])*256+f[18]!=n-51)return false;
        uint32_t offset=get32(f+13);
        if(offset%WINDOW_CHUNK)return false;
        uint32_t page=offset/PAGE_BYTES*PAGE_BYTES,index=(offset-page)/WINDOW_CHUNK;
        if(!bits || page!=base || memcmp(nonce,f+5,8)) {clear();base=page;memcpy(nonce,f+5,8);}
        if(bits&(uint32_t(1)<<index))return lengths[index]==n && !memcmp(frames[index],f,n);
        memcpy(frames[index],f,n);lengths[index]=uint8_t(n);bits|=uint32_t(1)<<index;return true;
    }
    uint32_t available(uint32_t page,const uint8_t* session) const {
        return page==base && !memcmp(nonce,session,8)?bits:0;
    }
};
}
