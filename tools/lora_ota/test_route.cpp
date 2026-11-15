#include "route_frame.h"
#include "relay_diagnostics.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main() {
    uint8_t inner[227]{},out[256]{},hop[8]={'F','4','M','L','V','-','1','0'};routeota::View view;
    for(size_t size:{size_t(51),size_t(91),size_t(226)}) {
        memcpy(inner,"\x3c\xff\x7e\x02",4);inner[17]=0;inner[18]=uint8_t(size-51);
        for(uint8_t phase=0;phase<4;phase++) {
            auto n=routeota::envelope(phase,hop,inner,size,out);assert(n==size+13);
            assert(routeota::parse(out,n+16,view));assert(view.phase==phase && view.size==size);
            assert(!routeota::parse(out,n+15,view));
            out[4]=4;assert(!routeota::parse(out,n+16,view));
        }
    }
    assert(!routeota::envelope(0,hop,inner,227,out));assert(!routeota::envelope(4,hop,inner,51,out));
    assert(!routeota::parse(out,256,view));
    uint8_t restart[52]{},nonce[8]={1,2,3,4,5,6,7,8};
    restart[4]=routeota::TEST_RELAY_RESTART;memcpy(restart+5,nonce,8);
    assert(routeota::restartMatchesSession(restart,51,true,nonce));
    assert(!routeota::restartMatchesSession(restart,51,false,nonce));
    assert(!routeota::restartMatchesSession(restart,52,true,nonce));
    restart[5]^=1;assert(!routeota::restartMatchesSession(restart,51,true,nonce));restart[5]^=1;
    restart[16]=1;assert(!routeota::restartMatchesSession(restart,51,true,nonce));restart[16]=0;
    restart[18]=1;assert(!routeota::restartMatchesSession(restart,51,true,nonce));restart[18]=0;
    restart[4]=6;assert(!routeota::restartMatchesSession(restart,51,true,nonce));
    uint8_t arm[52]{};arm[4]=routeota::TEST_RELAY_FAULT;memcpy(arm+5,nonce,8);
    arm[14]=0;arm[15]=0x20;arm[18]=1;arm[35]=1; // page base8192, burst
    routeota::RelayFault fault;
    assert(!fault.arm(arm,52,false,nonce));assert(!fault.arm(arm,51,true,nonce));
    arm[5]^=1;assert(!fault.arm(arm,52,true,nonce));arm[5]^=1;
    arm[16]=1;assert(!fault.arm(arm,52,true,nonce));arm[16]=0;
    assert(fault.arm(arm,52,true,nonce));
    assert(!fault.afterBlock(4096,nonce));
    for(int i=0;i<15;i++)assert(!fault.afterBlock(8192,nonce));
    assert(fault.afterBlock(8192,nonce));assert(!fault.afterBlock(8192,nonce));
    fault.clear();assert(!fault.afterBlock(8192,nonce));
    arm[35]=2;assert(fault.arm(arm,52,true,nonce));
    assert(!fault.afterBitmap(8192,nonce,8192,0xffffffffU));
    assert(!fault.afterBitmap(8192,nonce,12288,0x7fffffffU));
    assert(!fault.afterBitmap(4096,nonce,12288,0xffffffffU));
    assert(fault.afterBitmap(8192,nonce,12288,0xffffffffU));
    fault.clear();assert(!fault.afterBitmap(8192,nonce,12288,0xffffffffU));
    // Chain C -> R1 -> R2 -> T. Each frame on the air must be taken by exactly
    // the next station of its direction, whoever else overhears it.
    {
        const char *C="F4MLV-2",*R1="F4MLV-10",*R2="F4MLV-11",*T="F4MLV-15";
        struct Station {const char* name;const char* previous;const char* next;bool target;};
        const Station stations[]={{R1,C,R2,false},{R2,R1,T,false},{T,R2,nullptr,true}};
        auto listeners=[&](uint8_t phase,const char* from,bool down,const char*& taker) {
            uint8_t source[8]{};memcpy(source,from,strlen(from));
            memcpy(inner,"\x3c\xff\x7e\x02",4);inner[17]=0;inner[18]=0;
            auto n=routeota::envelope(phase,source,inner,51,out);assert(n);
            assert(routeota::parse(out,n+16,view));
            int count=0;taker=nullptr;
            for(const auto& s:stations) {
                bool takes=false;
                if(s.target)takes=down && view.phase==routeota::RELAY_TARGET && routeota::named(view.hopSource,s.previous);
                else takes=down?routeota::downstreamFrom(view,C,s.previous):routeota::upstreamFrom(view,T,s.next);
                if(takes) {++count;taker=s.name;}
            }
            return count;
        };
        const char* taker=nullptr;
        // Down: controller, then each relay re-emitting under its own name.
        assert(listeners(routeota::CONTROLLER_RELAY,C,true,taker)==1 && !strcmp(taker,R1));
        assert(listeners(routeota::RELAY_TARGET,R1,true,taker)==1 && !strcmp(taker,R2));
        assert(listeners(routeota::RELAY_TARGET,R2,true,taker)==1 && !strcmp(taker,T));
        // Up: target, then each relay; the last frame is for the controller only.
        assert(listeners(routeota::TARGET_RELAY,T,false,taker)==1 && !strcmp(taker,R2));
        assert(listeners(routeota::RELAY_CONTROLLER,R2,false,taker)==1 && !strcmp(taker,R1));
        assert(listeners(routeota::RELAY_CONTROLLER,R1,false,taker)==0);
        // A frame never travels against its direction or skips a hop.
        assert(listeners(routeota::RELAY_TARGET,R1,false,taker)==0);
        assert(listeners(routeota::RELAY_CONTROLLER,R2,true,taker)==0);
        assert(listeners(routeota::CONTROLLER_RELAY,R1,true,taker)==0);
        assert(listeners(routeota::TARGET_RELAY,R2,false,taker)==0);
        // A single relay keeps today's behaviour: controller and target as neighbours.
        uint8_t source[8]{};memcpy(source,C,strlen(C));
        auto n=routeota::envelope(routeota::CONTROLLER_RELAY,source,inner,51,out);
        assert(routeota::parse(out,n+16,view) && routeota::downstreamFrom(view,C,C));
        memset(source,0,8);memcpy(source,T,strlen(T));
        n=routeota::envelope(routeota::TARGET_RELAY,source,inner,51,out);
        assert(routeota::parse(out,n+16,view) && routeota::upstreamFrom(view,T,T));
        assert(!routeota::upstreamFrom(view,T,R2) && !routeota::downstreamFrom(view,C,C));
        // Neighbour sanity of a relay.
        assert(routeota::validNeighbours(R1,C,T,C,T));
        assert(routeota::validNeighbours(R1,C,T,C,R2));
        assert(routeota::validNeighbours(R2,C,T,R1,T));
        assert(!routeota::validNeighbours(R1,C,T,R1,T));  // itself upstream
        assert(!routeota::validNeighbours(R1,C,T,C,R1));  // itself downstream
        assert(!routeota::validNeighbours(R1,C,T,R2,R2)); // same neighbour twice
        assert(!routeota::validNeighbours(R1,C,T,T,R2));  // target upstream
        assert(!routeota::validNeighbours(R1,C,T,R2,C));  // controller downstream
    }
    puts("route framing: all four phases, limits, malformed lengths and relay chain passed");
}
