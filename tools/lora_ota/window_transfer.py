"""Page bursts, bitmap repair, and authenticated restart from durable progress."""
import struct
import time

START,DATA,QUERY,FINISH,ACTIVATE=range(15,20)
PAGE,CHUNK=4096,128


def transfer(client,image,sha,event,fault_checks=False,exercise_resume=False,exercise_relay_restart=False,
             relay_restart_point='cache'):
    if relay_restart_point not in ('cache','burst','ack'):raise ValueError('invalid relay restart point')
    if exercise_relay_restart and len(image)<3*PAGE:
        raise ValueError('relay restart test requires at least three pages')
    manifest=struct.pack('>I',len(image))+sha+struct.pack('>I',0x54474732)
    initial=client.request(START,0,manifest,timeout=12)
    position=initial['written']
    if exercise_relay_restart and position!=0:
        raise RuntimeError('relay restart test requires a fresh transfer')
    if position>len(image) or position!=len(image) and position%PAGE:
        raise RuntimeError('invalid durable receiver offset')
    event('durable_progress',written=position,total=len(image))
    if fault_checks and position==0:
        # Diagnostics only: one loss on each link, both reported separately.
        client.request(20,0,struct.pack('>I',1<<5))
        event('inject_downstream_loss',block=5)
    start_offset=position
    started=time.monotonic()
    resume_exercised=False
    repairs=0
    recoveries=0
    recovery_seconds=0
    relay_restart_injected=False
    relay_restart_recovered=False
    while position<len(image):
        length=min(PAGE,len(image)-position)
        mask=(1<<((length+CHUNK-1)//CHUNK))-1
        missing=mask
        try:
            if exercise_relay_restart and relay_restart_point!='cache' and not relay_restart_injected and position==2*PAGE:
                client.send_unacknowledged(22,position,bytes([1 if relay_restart_point=='burst' else 2]))
                relay_restart_injected=True
                event('relay_restart_armed',point=relay_restart_point,durable=position)
            for attempt in range(16):
                for index in range(32):
                    if not missing&(1<<index):continue
                    if fault_checks and position==0 and attempt==0 and index==7:
                        event('inject_upstream_loss',block=index)
                        continue
                    offset=position+index*CHUNK
                    client.send_unacknowledged(DATA,offset,image[offset:offset+CHUNK])
                    if exercise_relay_restart and relay_restart_point=='cache' and not relay_restart_injected and position==2*PAGE and index==15:
                        # The first half-page is in relay RAM, not yet forwarded.
                        client.send_unacknowledged(21,0,b'')
                        relay_restart_injected=True
                        event('relay_restart_requested',durable=position,cached_bytes=16*CHUNK)
                reply=client.request(QUERY,position,struct.pack('>I',mask),timeout=getattr(client,'window_timeout',20),attempts=2)
                bitmap=reply['bitmap']
                if bitmap&~mask or reply['written']>len(image):raise RuntimeError('invalid page bitmap')
                if bitmap==mask and reply['written']==position+length:break
                missing=mask&~bitmap
                if not missing:raise RuntimeError('complete bitmap without durable commit')
                repairs+=missing.bit_count()
                event('repair_page',offset=position,missing=missing,attempt=attempt+1)
            else:raise RuntimeError('page repair attempts exhausted')
        except TimeoutError:
            recoveries+=1
            if recoveries>5:raise
            event('link_lost',durable=position,recovery=recoveries)
            recovery_started=time.monotonic()
            client.recover_fast_session()
            restored=client.request(START,0,manifest,timeout=12)
            resumed=restored['written']
            if relay_restart_injected and not relay_restart_recovered:
                expected=(3 if relay_restart_point=='ack' else 2)*PAGE
                if resumed!=expected:raise RuntimeError('unexpected durable offset after relay restart')
                relay_restart_recovered=True
                event('relay_restart_recovered',written=resumed)
            if resumed<position or resumed>position+length or resumed!=len(image) and resumed%PAGE:
                raise RuntimeError('unexpected durable offset after link recovery')
            position=resumed
            recovery_seconds+=time.monotonic()-recovery_started
            event('link_recovered',written=position)
            continue
        position+=length
        event('page_committed',written=position,total=len(image))
        if exercise_resume and not resume_exercised and position>=2*PAGE and position+PAGE<len(image):
            # Flush half a page through the relay, then reboot the receiver.
            # The resume offset must exclude this uncommitted RAM buffer.
            partial_mask=0xffff
            for attempt in range(4):
                for index in range(16):
                    if partial_mask&(1<<index):
                        offset=position+index*CHUNK
                        client.send_unacknowledged(DATA,offset,image[offset:offset+CHUNK])
                partial=client.request(QUERY,position,struct.pack('>I',0xffffffff),timeout=20)
                if partial['written']!=position or partial['bitmap']&~0xffff:
                    raise RuntimeError('unexpected progress before restart')
                partial_mask=0xffff&~partial['bitmap']
                if not partial_mask:break
            else:raise RuntimeError('partial page was not staged before restart')
            client.request(13)  # authenticated diagnostic reset, after the ACK
            event('receiver_restart_requested',durable=position,volatile_bytes=16*CHUNK)
            recovery_started=time.monotonic()
            client.recover_fast_session()
            restored=client.request(START,0,manifest,timeout=12)
            if restored['written']!=position:raise RuntimeError('durable progress lost after restart')
            recovery_seconds+=time.monotonic()-recovery_started
            event('persistent_resume_confirmed',written=position)
            resume_exercised=True
    if exercise_relay_restart and not relay_restart_recovered:
        raise RuntimeError('relay restart did not exercise link recovery')
    result=client.request(FINISH,len(image),timeout=12)
    if result['written']!=len(image):raise RuntimeError('incomplete finished image')
    event('verified',elapsed_s=time.monotonic()-started,recovery_s=recovery_seconds,start_offset=start_offset,
          repairs=repairs,retries=client.retries,reply={k:(v.hex() if isinstance(v,bytes) else v) for k,v in result.items()})
    return result
