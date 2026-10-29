#include "profile_lease.h"
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
    puts("profile: bounds, ACK retry, lease renewal, explicit return, expiry and millis wrap passed");
}
