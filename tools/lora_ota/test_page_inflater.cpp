// Host test for the compressed page container: build it with zlib raw deflate,
// feed it in 4096-byte transport pages, interrupt and resume, check the image.
#include "page_inflater.h"
#include <zlib.h>
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>
using namespace labota;

struct Flash {std::vector<uint8_t> bytes;};
static size_t inflateRaw(void*,uint8_t* out,size_t capacity,const uint8_t* in,size_t n) {
    z_stream s{};if(inflateInit2(&s,-15)!=Z_OK)return SIZE_MAX;
    s.next_in=const_cast<uint8_t*>(in);s.avail_in=unsigned(n);s.next_out=out;s.avail_out=unsigned(capacity);
    int r=inflate(&s,Z_FINISH);size_t produced=capacity-s.avail_out;bool all=s.avail_in==0;inflateEnd(&s);
    return r==Z_STREAM_END && all?produced:SIZE_MAX;
}
static bool writeFlash(void* c,uint32_t offset,const uint8_t* data,size_t n) {
    auto& f=static_cast<Flash*>(c)->bytes;if(offset+n>f.size())return false;memcpy(f.data()+offset,data,n);return true;
}
static std::vector<uint8_t> container(const std::vector<uint8_t>& image) {
    std::vector<uint8_t> c;
    for(size_t i=0;i<image.size();i+=4096) {
        size_t n=std::min<size_t>(4096,image.size()-i);
        z_stream s{};assert(deflateInit2(&s,9,Z_DEFLATED,-15,9,Z_DEFAULT_STRATEGY)==Z_OK);
        std::vector<uint8_t> out(deflateBound(&s,n));
        s.next_in=const_cast<uint8_t*>(image.data()+i);s.avail_in=unsigned(n);s.next_out=out.data();s.avail_out=unsigned(out.size());
        assert(deflate(&s,Z_FINISH)==Z_STREAM_END);size_t L=out.size()-s.avail_out;deflateEnd(&s);
        c.push_back(uint8_t(L>>8));c.push_back(uint8_t(L));c.insert(c.end(),out.begin(),out.begin()+L);
    }
    return c;
}
static void run(const std::vector<uint8_t>& image,const std::vector<uint8_t>& c,int interruptAt) {
    Flash flash;flash.bytes.assign(image.size(),0xff);
    PageInflater p;p.inflate=inflateRaw;p.write=writeFlash;p.context=&flash;p.reset(uint32_t(image.size()),uint32_t(c.size()));
    uint32_t base=0;int pages=0;
    while(base<c.size()) {
        size_t n=std::min<size_t>(4096,c.size()-base);
        assert(p.feed(base,c.data()+base,n));base+=uint32_t(n);++pages;
        if(pages==interruptAt) {
            // Reset: only consumed and out survive; restart at the resume page.
            uint32_t consumed=p.consumed,out=p.out;
            PageInflater q;q.inflate=inflateRaw;q.write=writeFlash;q.context=&flash;q.reset(uint32_t(image.size()),uint32_t(c.size()),consumed,out);
            assert(q.resumeOffset()<=base && base-q.resumeOffset()<=4096+4096);
            base=q.resumeOffset();p=q;
            // A page already fully processed is accepted again without effect.
            if(base>=4096)assert(p.feed(base-4096,c.data()+base-4096,4096));
        }
    }
    assert(p.complete());assert(flash.bytes==image);
}
int main() {
    std::mt19937 rng(20261107);
    // Mixed content: compressible code-like bytes, random tables and erased padding.
    std::vector<uint8_t> image(5*4096+1234);
    for(size_t i=0;i<image.size();++i)image[i]=i<2*4096?uint8_t(i*7%31):i<4*4096?uint8_t(rng()):0xff;
    auto c=container(image);
    for(int cut:{0,1,2,3,5})run(image,c,cut);
    // An incompressible page makes a record longer than a transport page.
    std::vector<uint8_t> random(3*4096);for(auto& b:random)b=uint8_t(rng());
    auto rc=container(random);assert(rc.size()>random.size());
    for(int cut:{0,1,2})run(random,rc,cut);
    // Refusals: a gap in the pages, a damaged length and a wrong page size.
    {Flash f;f.bytes.assign(image.size(),0xff);PageInflater p;p.inflate=inflateRaw;p.write=writeFlash;p.context=&f;
     p.reset(uint32_t(image.size()),uint32_t(c.size()));assert(!p.feed(4096,c.data()+4096,4096));}
    {auto bad=c;bad[0]^=0x40;Flash f;f.bytes.assign(image.size(),0xff);PageInflater p;p.inflate=inflateRaw;p.write=writeFlash;p.context=&f;
     p.reset(uint32_t(image.size()),uint32_t(bad.size()));bool ok=true;
     for(uint32_t b=0;b<bad.size()&&ok;b+=4096)ok=p.feed(b,bad.data()+b,std::min<size_t>(4096,bad.size()-b));
     assert(!ok||!p.complete());}
    {Flash f;f.bytes.assign(image.size(),0xff);PageInflater p;p.inflate=inflateRaw;p.write=writeFlash;p.context=&f;
     p.reset(uint32_t(image.size()-1),uint32_t(c.size()));bool ok=true;
     for(uint32_t b=0;b<c.size()&&ok;b+=4096)ok=p.feed(b,c.data()+b,std::min<size_t>(4096,c.size()-b));
     assert(!ok||!p.complete());}
    std::printf("page inflater: container round trip, resume after reset, incompressible pages and refusals passed\n");
}
