#include "route_frame.h"
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
    puts("route framing: all four phases, limits and malformed lengths passed");
}
