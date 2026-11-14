#include "ota_rendezvous.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
int main(int argc,char**) {
    using R=labota::Rendezvous;
    R r;labota::RadioProfile p(434100000,125000,12,5,20);
    assert(r.poll(0)==R::NONE);
    assert(r.prepare(100,p));assert(!r.prepare(101,p));
    assert(!r.keep(102,1));assert(r.poll(2099)==R::NONE);
    assert(r.poll(2100)==R::ENTER && r.radio==p);
    assert(r.keep(2200,1));assert(!r.keep(3000,1));assert(!r.keep(3000,0));
    assert(r.poll(62199)==R::NONE);assert(r.poll(62200)==R::RESTORE);
    assert(!r.keep(62201,2));
    assert(r.prepare(100000,p));assert(r.poll(102000)==R::ENTER);
    r.leave(103000);r.leave(104000); // repeated close does not delay restoration
    assert(r.poll(104999)==R::NONE);assert(r.poll(105000)==R::RESTORE);
    assert(r.prepare(0,p));assert(r.poll(2000)==R::ENTER);
    for(uint32_t t=20000;t<7200000;t+=20000)assert(r.keep(t,t));
    assert(!r.keep(7200000,7200000));assert(r.poll(7200000)==R::RESTORE);
    const uint32_t start=UINT32_MAX-1000;
    assert(r.prepare(start,p));assert(r.poll(start+2000)==R::ENTER);
    assert(r.keep(start+3000,1));assert(r.poll(start+63000)==R::RESTORE);
    assert(r.prepare(0,p));r.reset();assert(r.poll(2000)==R::NONE);
    uint8_t state[31]{};
    labota::encodeCoordinationState(state,2,true,false,true,
        labota::RadioProfile(433775000,125000,12,5,20),p,17);
    assert(state[0]==1 && state[1]==2 && state[2]==1 && state[4]==1);
    assert(labota::RadioProfile::decode(state+5)==labota::RadioProfile(433775000,125000,12,5,20));
    assert(labota::RadioProfile::decode(state+16)==p);
    assert(labota::RadioProfile::read32(state+27)==17);
    if(argc>1) {for(uint8_t byte:state)printf("%02x",byte);puts("");return 0;}
    const std::string command="F4MLV-2>APGRWO,F4MLV-10,RFONLY::GENATX   :!RC1:A:3:OTA=?:tag";
    std::string forwarded;
    assert(labota::rendezvousControl(command,"F4MLV-2","GENATX","F4MLV-10",forwarded));
    assert(forwarded.find(",F4MLV-10*,RFONLY:")!=std::string::npos);
    assert(!labota::rendezvousControl(forwarded,"F4MLV-2","GENATX","F4MLV-10",forwarded));
    assert(!labota::rendezvousControl(command,"OTHER","GENATX","F4MLV-10",forwarded));
    assert(!labota::rendezvousControl(command,"F4MLV-2","OTHER","F4MLV-10",forwarded));
    assert(!labota::rendezvousControl("F4MLV-2>APGRWO,F4MLV-10::GENATX   :hello","F4MLV-2","GENATX","F4MLV-10",forwarded));
    assert(labota::rendezvousControl("GENATX>APLRG1,F4MLV-10,RFONLY::F4MLV-2  :OTA NORMAL ARMED=0 PROFILE=0","F4MLV-2","GENATX","F4MLV-10",forwarded));
    puts("rendezvous lease: all checks passed");
}
