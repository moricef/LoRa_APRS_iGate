#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace labota {
// Compressed image container: for each 4096-byte page of the image, in order,
// a big-endian 16-bit length L followed by L bytes of raw deflate that inflate
// to exactly min(4096, imageSize - pageStart) bytes. Pages are independent, so
// a reset loses at most the record being received.
//
// The transport carries this container in ordinary 4096-byte window pages,
// received in order. consumed is the container offset just after the last
// record written to flash; out is the image offset reached. Both are durable;
// a resumed transfer restarts at the transport page containing consumed.
struct PageInflater {
    static constexpr uint32_t PAGE=4096;
    static constexpr size_t MAX_RECORD=2+PAGE+64;      // stored blocks add a few bytes
    // Returns the number of bytes produced, or SIZE_MAX on any decoding error.
    using InflateFn=size_t(*)(void* context,uint8_t* out,size_t capacity,const uint8_t* in,size_t n);
    using WriteFn=bool(*)(void* context,uint32_t offset,const uint8_t* data,size_t n);
    InflateFn inflate=nullptr;WriteFn write=nullptr;void* context=nullptr;
    uint32_t imageSize=0,containerSize=0,consumed=0,out=0;
    uint8_t carry[MAX_RECORD+PAGE]{};size_t carryLength=0;
    uint8_t page[PAGE]{};

    void reset(uint32_t image,uint32_t container,uint32_t consumedBytes=0,uint32_t outBytes=0) {
        imageSize=image;containerSize=container;consumed=consumedBytes;out=outBytes;carryLength=0;
    }
    // First transport page to send again after a reset.
    uint32_t resumeOffset() const {return consumed==containerSize?consumed:consumed-consumed%PAGE;}
    bool complete() const {return consumed==containerSize && out==imageSize && carryLength==0;}
    // Feed one complete transport page that starts at container offset base.
    bool feed(uint32_t base,const uint8_t* data,size_t n) {
        if(!inflate||!write||!n||base>containerSize||n>containerSize-base)return false;
        uint32_t next=consumed+uint32_t(carryLength);          // first container byte not yet held
        if(base+n<=next)return true;                           // already processed
        if(base>next)return false;                             // a gap: pages must arrive in order
        size_t skip=next-base,add=n-skip;
        if(carryLength+add>sizeof carry)return false;
        memcpy(carry+carryLength,data+skip,add);carryLength+=add;
        while(carryLength>=2) {
            size_t length=(size_t(carry[0])<<8)|carry[1];
            if(!length||2+length>MAX_RECORD||out>=imageSize)return false;
            if(carryLength<2+length)break;
            size_t expected=std::min<uint32_t>(PAGE,imageSize-out);
            if(inflate(context,page,expected,carry+2,length)!=expected)return false;
            if(!write(context,out,page,expected))return false;
            out+=uint32_t(expected);consumed+=uint32_t(2+length);
            carryLength-=2+length;memmove(carry,carry+2+length,carryLength);
        }
        return true;
    }
};
}
