#pragma once
#include "session.h"
#include <algorithm>

namespace labota {
constexpr uint32_t PAGE_BYTES=4096, WINDOW_CHUNK=128, WINDOW_BLOCKS=32;
constexpr uint8_t WINDOW_START=15, WINDOW_DATA=16, WINDOW_QUERY=17,
                  WINDOW_FINISH=18, WINDOW_ACTIVATE=19;
struct WindowManifest {
    uint32_t size=0;
    uint8_t sha[32]{};
    bool same(const WindowManifest& other) const {
        return size==other.size && !memcmp(sha,other.sha,32);
    }
};
struct WindowStore {
    virtual ~WindowStore()=default;
    virtual uint32_t capacity() const=0;
    virtual bool load(WindowManifest&,uint32_t& committed)=0;
    // A checkpoint is durable and atomic. It follows the complete page write.
    virtual bool checkpoint(const WindowManifest&,uint32_t committed)=0;
    virtual bool page(uint32_t offset,const uint8_t*,size_t)=0;
    virtual bool same(uint32_t offset,const uint8_t*,size_t)=0;
    virtual bool finish(const WindowManifest&)=0;
    virtual bool activate(const WindowManifest&)=0;
    virtual bool discard()=0;
};
struct WindowSession {
    WindowStore& store;
    WindowManifest manifest;
    State state=IDLE;
    uint32_t written=0,bits=0;
    uint8_t buffer[PAGE_BYTES]{};
    explicit WindowSession(WindowStore& s):store(s) {}
    static uint32_t mask(uint32_t bytes) {
        uint32_t count=(bytes+WINDOW_CHUNK-1)/WINDOW_CHUNK;
        return count>=32?UINT32_MAX:count?(uint32_t(1)<<count)-1:0;
    }
    uint32_t pageSize(uint32_t base) const {
        return base<manifest.size?std::min(PAGE_BYTES,manifest.size-base):0;
    }
    uint32_t bitmap(uint32_t base) const {
        if(base%PAGE_BYTES || base>=manifest.size)return 0;
        return base<written?mask(pageSize(base)):base==written?bits:0;
    }
    Status start(const uint8_t* data,size_t n) {
        if(n!=40 || get32(data+36)!=BOARD)return FORMAT;
        WindowManifest wanted;wanted.size=get32(data);memcpy(wanted.sha,data+4,32);
        if(!wanted.size || wanted.size>store.capacity())return FORMAT;
        if(state!=IDLE)return manifest.same(wanted)?OK:BUSY;
        WindowManifest saved;uint32_t committed=0;
        if(store.load(saved,committed)) {
            if(!saved.same(wanted))return BUSY; // explicit abort is required to replace an image
            if(committed>wanted.size || (committed!=wanted.size && committed%PAGE_BYTES))return STORAGE;
        } else {
            committed=0;
            if(!store.checkpoint(wanted,0))return STORAGE;
        }
        manifest=wanted;written=committed;bits=0;state=RECEIVING;
        memset(buffer,0xff,sizeof buffer);return OK;
    }
    Status data(uint32_t offset,const uint8_t* bytes,size_t n) {
        if(state!=RECEIVING)return ORDER;
        if(offset%WINDOW_CHUNK || offset>=manifest.size ||
           n!=std::min(WINDOW_CHUNK,manifest.size-offset))return FORMAT;
        if(offset<written)return n<=written-offset && store.same(offset,bytes,n)?OK:ORDER;
        if(offset-written>=PAGE_BYTES)return ORDER;
        uint32_t index=(offset-written)/WINDOW_CHUNK,bit=uint32_t(1)<<index;
        if(bits&bit)return !memcmp(buffer+index*WINDOW_CHUNK,bytes,n)?OK:ORDER;
        memcpy(buffer+index*WINDOW_CHUNK,bytes,n);bits|=bit;
        if(bits==mask(pageSize(written))) {
            uint32_t end=written+pageSize(written);
            if(!store.page(written,buffer,pageSize(written)) || !store.checkpoint(manifest,end)) {
                // Retry must rewrite the entire page, never advertise an uncommitted page.
                bits=0;memset(buffer,0xff,sizeof buffer);return STORAGE;
            }
            written=end;bits=0;memset(buffer,0xff,sizeof buffer);
        }
        return OK;
    }
    Status finish() {
        if(state==READY)return OK;
        if(state!=RECEIVING || written!=manifest.size)return ORDER;
        if(!store.finish(manifest))return VERIFY;
        state=READY;return OK;
    }
    Status activate() {
        if(state!=READY)return ORDER;
        if(!store.activate(manifest))return STORAGE;
        state=ACTIVATING;return OK;
    }
    Status discard() {
        if(!store.discard())return STORAGE;
        state=IDLE;manifest={};written=bits=0;return OK;
    }
    void suspend() {state=IDLE;manifest={};written=bits=0;}
};
}
