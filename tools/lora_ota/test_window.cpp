#include "window_session.h"
#include "page_cache.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace labota;
struct MemoryPages : WindowStore {
    std::vector<uint8_t> flash=std::vector<uint8_t>(12288,0xff);
    WindowManifest record;uint32_t committed=0,writes=0;
    bool saved=false,failPage=false,failCheckpoint=false,valid=true,boot=false;
    uint32_t capacity() const override {return flash.size();}
    bool load(WindowManifest& m,uint32_t& c) override {if(!saved)return false;m=record;c=committed;return true;}
    bool checkpoint(const WindowManifest& m,uint32_t c) override {
        if(failCheckpoint)return false;
        record=m;committed=c;saved=true;return true;
    }
    bool page(uint32_t off,const uint8_t* p,size_t n) override {
        ++writes;memset(flash.data()+off,0xff,PAGE_BYTES);
        memcpy(flash.data()+off,p,failPage?n/2:n);return !failPage;
    }
    bool same(uint32_t o,const uint8_t* p,size_t n) override {return !memcmp(flash.data()+o,p,n);}
    bool finish(const WindowManifest&) override {return valid;}
    bool activate(const WindowManifest&) override {boot=true;return true;}
    bool discard() override {saved=false;return true;}
};
int main() {
    constexpr uint32_t size=2*PAGE_BYTES+73;
    std::vector<uint8_t> image(size);for(uint32_t i=0;i<size;i++)image[i]=uint8_t(i*7+i/17);
    uint8_t manifest[40]{};put32(manifest,size);manifest[4]=42;put32(manifest+36,BOARD);
    MemoryPages store;WindowSession first(store);
    assert(first.start(manifest,39)==FORMAT && !store.saved);
    assert(first.start(manifest,40)==OK && store.committed==0);
    auto block=[&](WindowSession& s,uint32_t i) {return s.data(i,image.data()+i,std::min(WINDOW_CHUNK,size-i));};
    // First page arrives in reverse order, with a hole and a duplicate.
    for(int i=31;i>=0;i--)if(i!=7)assert(block(first,i*WINDOW_CHUNK)==OK);
    assert(first.written==0 && first.bitmap(0)==(UINT32_MAX^(1u<<7)));
    assert(block(first,0)==OK && store.writes==0);
    assert(first.data(0,image.data()+128,128)==ORDER);
    assert(block(first,PAGE_BYTES)==ORDER);
    assert(first.finish()==ORDER && first.activate()==ORDER && !store.boot);
    assert(block(first,7*WINDOW_CHUNK)==OK && first.written==PAGE_BYTES && store.writes==1);
    assert(first.bitmap(0)==UINT32_MAX && block(first,0)==OK && store.writes==1);
    // Power loss midway through next page: only page zero is durable.
    for(uint32_t i=PAGE_BYTES;i<PAGE_BYTES+512;i+=128)assert(block(first,i)==OK);
    WindowSession resumed(store);assert(resumed.start(manifest,40)==OK && resumed.written==PAGE_BYTES);
    assert(resumed.bitmap(PAGE_BYTES)==0);
    uint8_t other[40];memcpy(other,manifest,40);other[4]++;
    WindowSession stranger(store);assert(stranger.start(other,40)==BUSY);
    // Cut power during flash programming. The checkpoint must stay on page zero.
    store.failPage=true;
    for(uint32_t i=PAGE_BYTES;i<2*PAGE_BYTES;i+=128)
        assert(block(resumed,i)==(i==2*PAGE_BYTES-128?STORAGE:OK));
    assert(store.committed==PAGE_BYTES && resumed.bits==0);
    store.failPage=false;WindowSession restart(store);assert(restart.start(manifest,40)==OK);
    // Cut power after page programming but before journal commit.
    store.failCheckpoint=true;
    for(uint32_t i=PAGE_BYTES;i<2*PAGE_BYTES;i+=128)
        assert(block(restart,i)==(i==2*PAGE_BYTES-128?STORAGE:OK));
    assert(store.committed==PAGE_BYTES);
    store.failCheckpoint=false;WindowSession again(store);assert(again.start(manifest,40)==OK);
    for(uint32_t i=PAGE_BYTES;i<size;i+=128)assert(block(again,i)==OK);
    assert(again.written==size && store.committed==size);
    assert(!memcmp(image.data(),store.flash.data(),size));
    store.valid=false;assert(again.finish()==VERIFY && !store.boot);
    store.valid=true;assert(again.finish()==OK && again.activate()==OK && store.boot);
    WindowSession completed(store);assert(completed.start(manifest,40)==OK && completed.written==size);
    assert(completed.finish()==OK);assert(completed.discard()==OK && !store.saved);
    PageCache cache;uint8_t f[179]{};memcpy(f,"\x3c\xff\x7e\x02",4);f[4]=WINDOW_DATA;f[5]=9;f[18]=128;
    assert(cache.put(f,sizeof f));assert(cache.available(0,f+5)==1);
    assert(cache.put(f,sizeof f));f[35]++;assert(!cache.put(f,sizeof f));f[35]--;
    put32(f+13,PAGE_BYTES);assert(cache.put(f,sizeof f) && cache.available(0,f+5)==0);
    f[5]++;assert(cache.available(PAGE_BYTES,f+5)==0);
    puts("window: reorder, loss, duplicates, atomic page checkpoint, interrupted flash/journal, restart, image binding and relay cache passed");
}
