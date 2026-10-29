#!/usr/bin/env python3
"""OTA v2: authenticated radio arm, image transfer and radio boot confirmation.

The target's USB port is neither opened nor required by this program.
"""
import argparse
import base64
import hashlib
import hmac
import json
import os
from pathlib import Path
import struct
import time
from send import open_serial

PREFIX=b'\x3c\xff\x7e\x02'
DOMAIN=b'LORA-APRS-OTA2'
START,DATA,FINISH,ACTIVATE,ABORT,HELLO,CONFIRM,STATUS,TEST_FAIL_ONCE,REACTIVATE=range(1,11)
PREPARE_FAST,RETURN_NORMAL=11,12
ACK=128
CHUNK=204
BOARD=0x54474732

def make_frame(key,source,target,nonce,op,offset,payload=b''):
    if len(source)>8 or len(target)>8 or len(payload)>CHUNK:raise ValueError('size')
    raw=PREFIX+bytes([op])+nonce+struct.pack('>IH',offset,len(payload))+source.ljust(8,b'\0')+target.ljust(8,b'\0')+payload
    return raw+hmac.digest(key,DOMAIN+raw,'sha256')[:16]

def decode_reply(key,source,target,raw):
    if len(raw)<57 or raw[:5]!=PREFIX+bytes([ACK]):return None
    offset,n=struct.unpack('>IH',raw[13:19])
    if len(raw)!=51+n or raw[19:27]!=target.ljust(8,b'\0') or raw[27:35]!=source.ljust(8,b'\0'):return None
    if not hmac.compare_digest(raw[-16:],hmac.digest(key,DOMAIN+raw[:-16],'sha256')[:16]):return None
    p=raw[35:-16]
    result=dict(nonce=raw[5:13],offset=offset,op=p[0],status=p[1],written=struct.unpack('>I',p[2:6])[0])
    if len(p)>6:
        if len(p)<48 or p[6] not in (0,1):return None
        vl=p[15]
        if vl>40 or len(p)!=48+vl:return None
        try:version=p[16:16+vl].decode('ascii')
        except UnicodeDecodeError:return None
        result.update(pending=bool(p[6]),running=struct.unpack('>I',p[7:11])[0],boot=struct.unpack('>I',p[11:15])[0],
                      version=version,image_sha=p[16+vl:].hex())
    return result

class Client:
    def __init__(self,port,key,source,target,counter_file,event):
        self.serial=open_serial(port)
        self.serial.baudrate=921600
        self.key,self.source,self.target=key,source,target
        self.counter_file=counter_file;self.nonce=bytes(8);self.event=event;self.retries=0
        self.default_timeout=1;self.negotiated=False;self.fast=False
    def request(self,op,offset=0,payload=b'',expected=0,drop_ack=False,timeout=None,attempts=16):
        if timeout is None:timeout=self.default_timeout
        raw=make_frame(self.key,self.source,self.target,self.nonce if op!=HELLO else bytes(8),op,offset,payload)
        for attempt in range(attempts):
            self.serial.write(b'TX '+raw.hex().encode()+b'\n')
            deadline=time.monotonic()+timeout
            while time.monotonic()<deadline:
                line=self.serial.readline().strip()
                if not line.startswith(b'RX '):continue
                try:r=decode_reply(self.key,self.source,self.target,bytes.fromhex(line[3:].decode()))
                except (ValueError,IndexError,UnicodeDecodeError):continue
                if r is None or r['op']!=op or r['offset']!=offset:continue
                if op!=HELLO and r['nonce']!=self.nonce:continue
                if drop_ack and attempt==0:
                    self.event('drop_ack',op=op,offset=offset);break
                if r['status']!=expected:raise RuntimeError(f'Unexpected response: {r}')
                if op==HELLO:self.nonce=r['nonce']
                return r
            self.retries+=1;self.event('retry',op=op,offset=offset,attempt=attempt+1)
        raise TimeoutError(f'op={op} offset={offset}')
    def hello(self):
        previous=int(self.counter_file.read_text()) if self.counter_file.exists() else 0
        counter=max(int(time.time()),previous+1)
        if counter>0xffffffff:raise ValueError('counter exhausted')
        self.counter_file.write_text(str(counter)+'\n')
        response=self.request(HELLO,counter,attempts=80,timeout=12 if self.negotiated else 3)
        self.event('hello',**printable(response));return response

    def bridge_profile(self,fast):
        command=b'PROFILE '+(b'1' if fast else b'0')+b'\n'
        deadline=time.monotonic()+3;next_send=0
        expected=b'PROFILE '+(b'1' if fast else b'0')+b' OK'
        while time.monotonic()<deadline:
            if time.monotonic()>=next_send:
                self.serial.write(command);next_send=time.monotonic()+.5
            if self.serial.readline().strip()==expected:
                self.fast=fast;self.default_timeout=1 if fast else 12;return
        raise TimeoutError('bridge profile command')
    def enable_profiles(self):
        self.negotiated=True;self.bridge_profile(False)
    def enter_fast(self,lease=30,drop_prepare_ack=False):
        try:self.request(PREPARE_FAST,0,bytes([1])+struct.pack('>H',lease),timeout=12,attempts=1,drop_ack=drop_prepare_ack)
        except TimeoutError:self.event('prepare_ack_missing')
        self.bridge_profile(True);time.sleep(2.2)
        self.request(STATUS,attempts=8)
        self.event('fast_profile_confirmed',lease_seconds=lease)
    def return_normal(self):
        try:self.request(RETURN_NORMAL,attempts=3)
        except TimeoutError:self.event('return_ack_missing')
        self.bridge_profile(False);time.sleep(.8)
        self.request(STATUS,attempts=4)
        self.event('normal_profile_confirmed')
    def close(self):
        if self.negotiated and self.fast:
            try:self.return_normal()
            except Exception:
                # Receiver has its own lease even if the radio link has failed.
                self.bridge_profile(False)
        self.serial.close()

def printable(reply):return {k:(v.hex() if isinstance(v,bytes) else v) for k,v in reply.items()}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--bridge',required=True);ap.add_argument('--key-file',required=True,type=Path)
    ap.add_argument('--source',default='F4MLV-2');ap.add_argument('--target',default='F4MLV-15')
    ap.add_argument('--image',type=Path,required=True);ap.add_argument('--log',type=Path,required=True)
    ap.add_argument('--counter-file',type=Path,help='Persistent counter for this controller/target')
    ap.add_argument('--resume-log',type=Path,help='Resume the same authenticated receiver session from a previous log')
    ap.add_argument('--exercise-rollback',action='store_true')
    ap.add_argument('--exercise-confirm-timeout',action='store_true',help='Withhold confirmation, observe rollback, then activate again')
    ap.add_argument('--fault-checks',action='store_true')
    ap.add_argument('--negotiate-profile',action='store_true',help='Start at the bench normal profile; negotiate the temporary fast profile')
    args=ap.parse_args();image=args.image.read_bytes();sha=hashlib.sha256(image).digest()
    if args.negotiate_profile and args.resume_log:raise ValueError('profile resume not supported by this client')
    key=base64.urlsafe_b64decode(args.key_file.read_text().strip()+'=')
    if len(key)!=32 or image[:1]!=b'\xe9':raise ValueError('key/image')
    args.log.parent.mkdir(parents=True,exist_ok=True)
    with args.log.open('w') as log:
        def event(kind,**data):
            line=json.dumps(dict(time=time.time(),event=kind,**data));log.write(line+'\n');log.flush()
            if kind!='block':print(line,flush=True)
        counter_file=args.counter_file or args.log.parent/(args.target+'.counter')
        c=Client(args.bridge,key,args.source.encode(),args.target.encode(),counter_file,event)
        try:
            if args.negotiate_profile:c.enable_profiles()
            if args.resume_log:
                previous=[json.loads(x) for x in args.resume_log.read_text().splitlines()]
                old_image=next(x for x in previous if x['event']=='image')
                if old_image['bytes']!=len(image) or old_image['sha256']!=sha.hex():raise ValueError('resume image mismatch')
                old_hello=next(x for x in reversed(previous) if x['event']=='hello')
                c.nonce=bytes.fromhex(old_hello['nonce'])
                before=c.request(STATUS)
                event('hello',**printable(before))
            else:before=c.hello()
            assert not before['pending']
            if args.negotiate_profile:c.enter_fast()
            event('image',bytes=len(image),sha256=sha.hex(),initial=printable(before))
            manifest=struct.pack('>I',len(image))+sha+struct.pack('>I',BOARD)
            resume_offset=c.request(START,0,manifest,timeout=12)['written']
            assert resume_offset<=len(image) and (resume_offset==len(image) or resume_offset%CHUNK==0)
            if args.resume_log:event('resumed',offset=resume_offset,previous_log=str(args.resume_log))
            else:assert resume_offset==0
            if args.fault_checks and resume_offset==0:
                c.request(FINISH,len(image),expected=2)
                c.request(DATA,CHUNK,image[CHUNK:2*CHUNK],expected=2)
            started=time.monotonic()
            for offset in range(resume_offset,len(image),CHUNK):
                block=image[offset:offset+CHUNK]
                r=c.request(DATA,offset,block,drop_ack=args.fault_checks and offset==resume_offset)
                assert r['written']==offset+len(block)
                event('block',offset=offset,written=r['written'])
                if offset//CHUNK%500==0:print(f'{r["written"]}/{len(image)} bytes',flush=True)
            r=c.request(FINISH,len(image));assert r['running']==before['running'] and r['boot']==before['boot']
            event('verified',elapsed_s=time.monotonic()-started,start_offset=resume_offset,retries=c.retries,reply=printable(r))
            if args.negotiate_profile:c.return_normal()
            if args.exercise_rollback:c.request(TEST_FAIL_ONCE)
            try:c.request(ACTIVATE,len(image),attempts=3,timeout=12 if c.negotiated else 3)
            except TimeoutError:event('activation_ack_missing')
            time.sleep(5)
            new=c.hello()
            if args.exercise_rollback:
                assert new['running']==before['running'] and new['version']==before['version'] and not new['pending']
                event('rollback_confirmed',reply=printable(new))
                try:c.request(REACTIVATE,0,sha,attempts=3,timeout=12 if c.negotiated else 3)
                except TimeoutError:event('reactivation_ack_missing')
                time.sleep(5)
                new=c.hello()
            assert new['pending'] and new['running']!=before['running'] and new['image_sha']==sha.hex()
            event('boot_pending_confirmation',reply=printable(new))
            if args.exercise_confirm_timeout:
                event('confirmation_withheld')
                for elapsed in range(0,190,5):
                    time.sleep(5)
                    if elapsed%30==25:print('Withholding confirmation:',elapsed+5,'seconds',flush=True)
                fallback=c.hello()
                assert not fallback['pending'] and fallback['running']==before['running'] and fallback['version']==before['version']
                event('confirmation_timeout_rollback',reply=printable(fallback))
                try:c.request(REACTIVATE,0,sha,attempts=3,timeout=12 if c.negotiated else 3)
                except TimeoutError:event('reactivation_ack_missing')
                time.sleep(5);new=c.hello()
                assert new['pending'] and new['running']!=before['running'] and new['image_sha']==sha.hex()
                event('boot_pending_confirmation',reply=printable(new))
            r=c.request(CONFIRM,0,sha);assert not r['pending']
            # Retry the same confirmation deliberately: an ACK loss must not
            # turn an already successful confirmation into an error.
            r=c.request(CONFIRM,0,sha);assert not r['pending']
            event('radio_confirmed',reply=printable(r))
            c.request(ABORT)
        finally:
            c.close()

if __name__=='__main__':main()
