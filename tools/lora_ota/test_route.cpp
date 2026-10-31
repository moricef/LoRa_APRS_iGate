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
    puts("route framing: all four phases, limits and malformed lengths passed");
}
