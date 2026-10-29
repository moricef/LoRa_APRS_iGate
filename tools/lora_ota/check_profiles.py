#!/usr/bin/env python3
"""Direct-RF tests of a temporary profile and its recovery lease."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import struct
import time
from send_integrated import Client,printable,START,DATA,STATUS,ABORT,PREPARE_FAST,RETURN_NORMAL,BOARD

TEST_REBOOT=13

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('bridge','key-file','counter-file','image','log'):ap.add_argument('--'+name,required=True)
    args=ap.parse_args()
    key=base64.urlsafe_b64decode(Path(args.key_file).read_text().strip()+'=')
    image=Path(args.image).read_bytes()
    with Path(args.log).open('w') as log:
        def event(kind,**fields):
            row=json.dumps(dict(time=time.time(),event=kind,**fields));log.write(row+'\n');log.flush();print(row,flush=True)
        c=Client(args.bridge,key,b'F4MLV-2',b'F4MLV-15',Path(args.counter_file),event)
        try:
            c.enable_profiles();initial=c.hello()
            assert not initial['pending']
            def normal_boot(kind):
                r=c.hello();assert r['version']==initial['version'] and r['running']==initial['running'] and not r['pending'] and r['written']==0
                event(kind,reply=printable(r))
            c.request(PREPARE_FAST,0,bytes([2,0,30]),expected=1)
            c.request(PREPARE_FAST,0,bytes([1,0,5]),expected=2)
            c.request(STATUS);event('invalid_profile_rejected_on_normal')
            c.enter_fast(15,drop_prepare_ack=True)
            c.return_normal();c.request(ABORT)
            event('lost_prepare_ack_recovered')
            normal_boot('normal_before_interruption')
            c.enter_fast(15)
            # Stop all valid RF traffic from the controller while the target is fast.
            c.bridge_profile(False);time.sleep(17)
            normal_boot('idle_lease_restored_normal')
            c.request(ABORT)
            normal_boot('normal_before_partial_transfer')
            c.enter_fast(15)
            manifest=struct.pack('>I',len(image))+hashlib.sha256(image).digest()+struct.pack('>I',BOARD)
            c.request(START,0,manifest,timeout=12)
            c.request(DATA,0,image[:204])
            c.bridge_profile(False);time.sleep(17)
            normal_boot('partial_transfer_expired_on_normal')
            c.request(ABORT)
            normal_boot('normal_before_receiver_reset')
            c.enter_fast(15)
            c.request(START,0,manifest,timeout=12);c.request(DATA,0,image[:204])
            c.request(TEST_REBOOT)
            c.bridge_profile(False);time.sleep(5)
            normal_boot('receiver_reset_restored_normal')
            c.request(ABORT)
            normal_boot('normal_before_lost_return_ack')
            c.enter_fast(15)
            try:c.request(RETURN_NORMAL,drop_ack=True,attempts=1)
            except TimeoutError:pass
            c.bridge_profile(False);time.sleep(.8)
            c.request(STATUS);event('lost_return_ack_recovered')
            c.request(ABORT)
            event('profile_checks_complete')
        finally:c.close()

if __name__=='__main__':main()
