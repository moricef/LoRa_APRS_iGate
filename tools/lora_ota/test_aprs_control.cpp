#include "ota_aprs_control.h"
#include "profile_lease.h"
#include <cassert>
#include <iostream>
using namespace labota;
int main() {
    AprsOtaCommand c;
    assert(AprsOtaCommand::parse("OTA=PROFILE,434550000,250000,8,6,3",c));
    assert(c.kind==AprsOtaCommand::PROFILE && c.radio.valid(RadioChip::SX1278));
    assert(c.radio==RadioProfile(434550000,250000,8,6,3));
    assert(AprsOtaCommand::parse("OTA=PROFILE,434550000,500000,5,5,-9",c));
    assert(!c.radio.valid(RadioChip::SX1278) && c.radio.valid(RadioChip::SX1262));
    for(const char* s:{"OTA=PROFILE,434550000,250000,8,6,3,1", "OTA=PROFILE,434550000,250000,8,6",
        "OTA=PROFILE,434550000,250000,8,6,3x", "OTA=PROFILE,4294967296,250000,8,6,3",
        "OTA=PROFILE,434550000,250000,264,6,3", "OTA=PROFILE,434550000,250000,8,6,259",
        "OTA=FAST,14", "OTA=FAST,61", "OTA=FAST,30,1", "OTA=FAST,+30", "OTA=NORMALx"})
        assert(!AprsOtaCommand::parse(s,c));
    assert(AprsOtaCommand::parse("OTA=?",c)&&c.kind==AprsOtaCommand::STATUS);
    assert(AprsOtaCommand::parse("OTA=NORMAL",c)&&c.kind==AprsOtaCommand::NORMAL);
    assert(AprsOtaCommand::parse("OTA=FAST,30",c)&&c.lease==30);
    ProfileLease lease;
    // Failed response: no prepare call, so no switch. Successful response:
    // prepare is called only after transmit completes, then the two-second delay.
    assert(lease.poll(10000)==ProfileLease::NONE);
    assert(lease.prepare(10000,c.lease));
    assert(lease.poll(11999)==ProfileLease::NONE);
    assert(lease.poll(12000)==ProfileLease::ENTER_FAST);
    assert(lease.poll(41999)==ProfileLease::NONE);
    assert(lease.poll(42000)==ProfileLease::RESTORE_NORMAL);
    std::cout<<"APRS OTA argument and lease checks passed\n";
}
