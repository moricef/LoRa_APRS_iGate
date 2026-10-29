#include "session.h"
#include <cassert>
#include <vector>
#include <cstdio>
using namespace labota;
struct Memory : Store {
    std::vector<uint8_t> data;
    bool boot=false,valid=true,writable=true;
    uint32_t capacity() const override { return 1000; }
    bool begin(uint32_t) override { data.clear();return true; }
    bool write(const uint8_t* p,size_t n) override {if(!writable)return false;data.insert(data.end(),p,p+n);return true;}
    bool same(uint32_t o,const uint8_t* p,size_t n) override {return o+n<=data.size()&&!memcmp(data.data()+o,p,n);}
    bool finish(const uint8_t*) override {return valid;}
    bool activate() override {boot=true;return true;}
    void abort() override {}
};
int main() {
    Memory m;Session s(m);uint8_t start[40]{},a[CHUNK]{1},b[CHUNK]{2};
    constexpr uint32_t N=CHUNK, TOTAL=2*N;
    put32(start,TOTAL);put32(start+36,BOARD);
    assert(s.accept(ACTIVATE,0,nullptr,0)==ORDER && !m.boot);
    assert(s.accept(START,0,start,39)==FORMAT);
    assert(s.accept(START,0,start,40)==OK);
    assert(s.accept(FINISH,TOTAL,nullptr,0)==ORDER && !m.boot);
    assert(s.accept(DATA,N,b,N)==ORDER);
    assert(s.accept(DATA,0,a,N)==OK);
    assert(s.accept(START,0,start,40)==OK && s.written==N);
    assert(s.accept(DATA,0,a,N)==OK && s.written==N);
    assert(s.accept(DATA,0,b,N)==ORDER);
    assert(s.accept(DATA,TOTAL,b,1)==FORMAT);
    assert(s.accept(DATA,N,b,N)==OK && s.written==TOTAL);
    m.valid=false;assert(s.accept(FINISH,TOTAL,nullptr,0)==VERIFY && s.state==IDLE && !m.boot);
    assert(s.accept(START,0,start,40)==OK);
    assert(s.accept(DATA,0,a,N)==OK);
    assert(s.accept(ABORT,0,nullptr,0)==OK && s.state==IDLE);
    assert(s.accept(START,0,start,40)==OK);
    m.writable=false;assert(s.accept(DATA,0,a,N)==STORAGE && s.state==IDLE);
    m.writable=true;m.valid=true;
    assert(s.accept(START,0,start,40)==OK);
    assert(s.accept(DATA,0,a,N)==OK && s.accept(DATA,N,b,N)==OK);
    assert(s.accept(FINISH,TOTAL,nullptr,0)==OK && s.state==READY && !m.boot);
    assert(s.accept(FINISH,TOTAL,nullptr,0)==OK);
    assert(s.accept(ACTIVATE,TOTAL,nullptr,0)==OK && m.boot);
    puts("session: ordering, duplicate retries, incomplete image, failed verification/write, abort and activation checks passed");
}
