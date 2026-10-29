#!/usr/bin/env python3
"""Post-update checks over radio and the existing TNC2 observation stream."""
import argparse
import base64
import json
from pathlib import Path
import socket
import time
from send_integrated import *

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--bridge',required=True);ap.add_argument('--key-file',type=Path,required=True)
    ap.add_argument('--counter-file',type=Path,required=True);ap.add_argument('--frames',type=Path,required=True)
    ap.add_argument('--tnc-host',required=True);ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--normal-profile',action='store_true',help='Use the SF12 normal profile of the profile-switch bench')
    ap.add_argument('--formats-only',action='store_true',help='Only repeat reception checks after verifying the running version')
    args=ap.parse_args();key=base64.urlsafe_b64decode(args.key_file.read_text().strip()+'=')
    results=[]
    def event(kind,**fields):
        r=dict(event=kind,**fields);results.append(r);print(json.dumps(r),flush=True)
    c=Client(args.bridge,key,b'F4MLV-2',b'F4MLV-15',args.counter_file,event)
    if args.normal_profile:c.enable_profiles()
    initial=c.hello();assert initial['version']=='4.0.2RXT-OTA-B' and not initial['pending']
    if not args.formats_only:
        used=int(args.counter_file.read_text());nonce=c.nonce
        duplicate=c.request(HELLO,used,timeout=12 if args.normal_profile else 3);assert duplicate['nonce']==nonce
        event('hello_retry_idempotent')
        c.request(ABORT)
        def no_reply(raw,op,offset):
            c.serial.write(b'TX '+raw.hex().encode()+b'\n');until=time.monotonic()+(12 if args.normal_profile else 2)
            while time.monotonic()<until:
                line=c.serial.readline().strip()
                if not line.startswith(b'RX '):continue
                try:r=decode_reply(key,c.source,c.target,bytes.fromhex(line[3:].decode()))
                except (ValueError,IndexError,UnicodeError):continue
                if r and r['op']==op and r['offset']==offset:raise AssertionError('unexpected authenticated reply')
        no_reply(make_frame(key,c.source,c.target,bytes(8),HELLO,used),HELLO,used)
        event('retired_hello_counter_rejected')
        wrong_counter=max(used+1,int(time.time()))
        no_reply(make_frame(bytes(32),c.source,c.target,bytes(8),HELLO,wrong_counter),HELLO,wrong_counter)
        event('wrong_key_rejected')
        fresh=c.hello();assert fresh['nonce']!=nonce
        no_reply(make_frame(key,c.source,c.target,nonce,DATA,0,b'test'),DATA,0)
        status=c.request(STATUS);assert status['written']==0
        event('previous_session_nonce_rejected')
        c.request(ABORT)
    else:c.request(ABORT)
    with socket.create_connection((args.tnc_host,8001),timeout=5) as stream:
        # TCP connect may complete before the firmware has accepted the client.
        time.sleep(1)
        stream.settimeout(.1);record=bytearray()
        for line in args.frames.read_text().splitlines():
            kind,raw=line.split('\t')
            marker={'text':b'OTA-TEXT-CHECK','native':b'OTA-NATIVE-CHECK','ppm':b'OTA-PPM-CHECK'}[kind]
            start=len(record)
            # This checks decoder availability, not a packet-loss rate.
            for attempt in range(1,4):
                c.serial.write(b'TXRAW '+raw.encode()+b'\n')
                until=time.monotonic()+(8 if args.normal_profile else 3)
                while time.monotonic()<until:
                    try:
                        data=stream.recv(4096)
                        if not data:raise RuntimeError('TNC closed')
                        record.extend(data)
                    except socket.timeout:pass
                if marker in record[start:]:break
                event('format_reception_missing',format=kind,attempt=attempt)
            assert marker in record[start:],(kind,bytes(record))
            event('format_received',format=kind,attempt=attempt)
        assert b'F4MLV-2<--F4MLV-9' in record, bytes(record)
        event('native_rxt_relay_association_preserved')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.with_suffix('.tnc').write_bytes(record)
    args.output.write_text(json.dumps(results,indent=2)+'\n')
    c.serial.close()

if __name__=='__main__':main()
