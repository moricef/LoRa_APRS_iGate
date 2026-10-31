#include "profile_lease.h"
#include "ota_radio_profile.h"
#include <cassert>
#include <cstdio>
using labota::ProfileLease;
int main() {
    ProfileLease p;
    assert(!p.prepare(100,14));assert(!p.prepare(100,61));
    assert(p.prepare(100,30));assert(p.prepare(500,30));assert(!p.prepare(500,20));
    assert(p.poll(2099)==p.NONE);assert(p.poll(2100)==p.ENTER_FAST);
    p.activity(3000);assert(p.poll(32999)==p.NONE);assert(p.poll(33000)==p.RESTORE_NORMAL);
    assert(p.mode==p.NORMAL);
    assert(p.prepare(100,15));p.requestReturn(200);p.requestReturn(600);
    assert(p.poll(699)==p.NONE);assert(p.poll(700)==p.RESTORE_NORMAL);
    assert(p.prepare(0xfffffc00U,15));assert(p.poll(975)==p.NONE);
    assert(p.poll(976)==p.ENTER_FAST);p.activity(0xfffffff0U);
    assert(p.poll(14983)==p.NONE);assert(p.poll(14984)==p.RESTORE_NORMAL);
    assert(p.prepare(0,30));p.reset();assert(p.poll(50000)==p.NONE);
    using labota::RadioProfile;using labota::RadioChip;using labota::FastProfileRequest;
    RadioProfile selected(434550000,250000,8,6,3);
    assert(selected.valid(RadioChip::SX1278));
    uint8_t encoded[14]={2,0,60};selected.encode(encoded+3);
    FastProfileRequest r;assert(FastProfileRequest::decode(encoded,sizeof encoded,r));
    assert(r.custom && r.lease==60 && r.radio==selected);
    assert(!FastProfileRequest::decode(encoded,13,r));assert(!FastProfileRequest::decode(encoded,3,r));
    encoded[2]=14;assert(!FastProfileRequest::decode(encoded,14,r));encoded[2]=60;
    uint8_t legacy[3]={1,0,30};assert(FastProfileRequest::decode(legacy,3,r) && !r.custom && r.lease==30);
    selected.sf=5;assert(!selected.valid(RadioChip::SX1278) && selected.valid(RadioChip::SX1262));
    selected.sf=6;assert(!selected.valid(RadioChip::SX1276));
    selected.sf=12;assert(!selected.valid(RadioChip::LLCC68));
    selected.sf=10;selected.bandwidthHz=125000;assert(!selected.valid(RadioChip::LLCC68));
    selected.bandwidthHz=250000;assert(selected.valid(RadioChip::LLCC68));
    selected.frequencyHz=915000000;assert(!selected.valid(RadioChip::SX1278) && selected.valid(RadioChip::SX1276));
    selected.frequencyHz=434550000;selected.bandwidthHz=12500;assert(!selected.valid(RadioChip::SX1278));
    selected.bandwidthHz=250000;selected.power=19;assert(!selected.valid(RadioChip::SX1278));
    puts("profile: bounds, ACK retry, lease renewal, explicit return, expiry and millis wrap passed");
}
