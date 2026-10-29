#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace labota {
constexpr size_t CHUNK = 204;
constexpr uint32_t BOARD = 0x54474732; // TTGO LoRa32 V2.1, experimental board id
enum Op : uint8_t { START=1, DATA=2, FINISH=3, ACTIVATE=4, ABORT=5, ACK=128 };
enum Status : uint8_t { OK=0, FORMAT=1, ORDER=2, STORAGE=3, VERIFY=4, BUSY=5 };
enum State { IDLE, RECEIVING, READY, ACTIVATING };
inline uint32_t get32(const uint8_t* p) {
    return uint32_t(p[0])<<24 | uint32_t(p[1])<<16 | uint32_t(p[2])<<8 | p[3];
}
inline void put32(uint8_t* p, uint32_t n) {
    for (int i=3;i>=0;--i) { p[i]=uint8_t(n); n>>=8; }
}
struct Store {
    virtual ~Store() = default;
    virtual uint32_t capacity() const = 0;
    virtual bool begin(uint32_t size) = 0;
    virtual bool write(const uint8_t*,size_t) = 0;
    virtual bool same(uint32_t offset,const uint8_t*,size_t) = 0;
    virtual bool finish(const uint8_t sha[32]) = 0;
    virtual bool activate() = 0;
    virtual void abort() = 0;
};
struct Session {
    Store& store;
    State state=IDLE;
    uint32_t size=0, written=0;
    uint8_t sha[32]{};
    explicit Session(Store& s):store(s) {}
    void cancel() { store.abort(); state=IDLE; size=written=0; }
    Status accept(uint8_t op,uint32_t offset,const uint8_t* data,size_t n) {
        if (op==ABORT) {
            if (n) return FORMAT;
            cancel(); return OK;
        }
        if (op==START) {
            if (n!=40 || offset || get32(data+36)!=BOARD) return FORMAT;
            uint32_t wanted=get32(data);
            if (!wanted || wanted>store.capacity()) return FORMAT;
            if (state!=IDLE) {
                return wanted==size && !std::memcmp(data+4,sha,32) ? OK : BUSY;
            }
            if (!store.begin(wanted)) return STORAGE;
            size=wanted; written=0; std::memcpy(sha,data+4,32); state=RECEIVING;
            return OK;
        }
        if (op==DATA) {
            if (state!=RECEIVING) return ORDER;
            if (!n || n>CHUNK || offset>size || n>size-offset) return FORMAT;
            if (offset<written) {
                return n<=written-offset && store.same(offset,data,n) ? OK : ORDER;
            }
            if (offset!=written) return ORDER;
            if (!store.write(data,n)) { cancel(); return STORAGE; }
            written+=n; return OK;
        }
        if (op==FINISH) {
            if (n || offset!=size) return FORMAT;
            if (state==READY) return OK;
            if (state!=RECEIVING || written!=size) return ORDER;
            if (!store.finish(sha)) { cancel(); return VERIFY; }
            state=READY; return OK;
        }
        if (op==ACTIVATE) {
            if (n || offset!=size || state!=READY) return ORDER;
            if (!store.activate()) return STORAGE;
            state=ACTIVATING; return OK;
        }
        return FORMAT;
    }
};
}
