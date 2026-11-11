#!/usr/bin/env python3
"""Plan/run matched block-size trials; never activate an application.

See docs/proposals/LORA_OTA_BLOCK_BENCH_fr.md. Radio conditions are established
externally and described, not simulated or inferred from successful delivery.
"""
import argparse,base64,csv,hashlib,json,math,random,statistics,time
from pathlib import Path
from urllib.request import urlopen
from radio_profiles import load_profiles
from send_integrated import Client,ABORT,NETWORK_STATUS,printable
from send import open_serial
from route_transport import RouteTransport
from window_transfer import transfer

PAGE=4096


def igate_bridge(port,baud=921600):
    """Raise an iGate OTA firmware's console to the bridge speed.

    The iGate starts at 115200 baud; the dedicated bridge firmware already runs
    at 921600 and ignores this line. Keep the successful connection open: with
    CH343 boards, closing it before the transfer resets the controller."""
    device=open_serial(port)
    device.reset_input_buffer();deadline=time.monotonic()+15;next_send=0
    while time.monotonic()<deadline:
        if time.monotonic()>=next_send:
            device.write(b'OTABRIDGE %d\n'%baud);device.flush();next_send=time.monotonic()+1
        if device.readline().strip()==b'OTABRIDGE OK':
            device.baudrate=baud
            time.sleep(1)
            device.reset_input_buffer()
            return device
    device.close()
    return None
DEFAULT_BLOCKS=(32,64,96,128,160,175)


def geometry(block,routed):
    maximum=175 if routed else 204
    if type(block) is not int or not 32<=block<=maximum:
        raise ValueError(f'block size must be 32..{maximum} for this topology')
    count=math.ceil(PAGE/block)
    return count,4*math.ceil(count/32)


def plan(blocks,repeats,seed,profiles,routed):
    if repeats<1:raise ValueError('repeats must be positive')
    if not blocks:raise ValueError('at least one block size required')
    if len(set(blocks))!=len(blocks):raise ValueError('duplicate block sizes')
    for profile in profiles.values():
        if not (137000000<=profile.frequency_hz<=525000000 and 7<=profile.spreading_factor<=12 and
                (2<=profile.power_dbm<=17 or profile.power_dbm==20)):
            raise ValueError('profile is incompatible with the SX1278 benchmark bridge')
    rng=random.Random(seed);rows=[];fast=profiles['transfer'];normal=profiles['normal']
    if 2*normal.airtime()+5>=60:raise ValueError('normal profile too slow for lease')
    if fast.airtime()+5>=60:raise ValueError('transfer frame too slow for lease')
    for block in blocks:
        count,width=geometry(block,routed);overhead=80 if routed else 51
        # Conservative waiting bound: target is silent while a relay fills RAM.
        silence=count*(fast.airtime(overhead+block)+.02)+5
        if routed and silence>=60:raise ValueError(f'block {block}: relay page burst exceeds the 60-second lease')
    for repetition in range(repeats):
        order=list(blocks);rng.shuffle(order)
        for block in order:
            count,width=geometry(block,routed);overhead=80 if routed else 51
            lengths=[min(block,PAGE-i*block) for i in range(count)]
            page_air=sum(fast.airtime(overhead+n) for n in lengths)
            page_air+=fast.airtime(overhead+width)+fast.airtime(overhead+6+width)
            rows.append(dict(repetition=repetition+1,block_bytes=block,blocks_per_page=count,
                             bitmap_bytes=width,data_frame_max_bytes=overhead+block,
                             ideal_page_airtime_s=page_air*(2 if routed else 1)))
    return rows


def counter_delta(before,after,boot):
    if before[boot]!=after[boot]:raise RuntimeError('equipment restarted during measured trial')
    fields=('tx_frames','tx_bytes','tx_airtime_us','tx_errors')
    out={k:after[k]-before[k] for k in fields}
    if any(v<0 for v in out.values()):raise RuntimeError('counter reset or wrap during trial')
    if out['tx_errors']:raise RuntimeError('radio driver reported TX errors')
    return out


def relay_stats(url,expected=None):
    if not url:raise ValueError('routed trials require --relay-status-url')
    with urlopen(url,timeout=5) as response:data=json.load(response)
    if data.get('role')!=1 or data.get('window_geometry')!=2:
        raise ValueError('relay does not support variable block sizes and TX accounting')
    if expected and any(data.get(k)!=v for k,v in expected.items()):
        raise ValueError('relay status URL identifies a different route')
    return dict(elf_sha256=data.get('elf_sha256'),boot_id=data['boot_id'],tx_frames=data['tx_frames'],tx_bytes=data['tx_bytes'],
                tx_airtime_us=data['tx_airtime_us'],tx_errors=data['tx_errors_radio'],
                local_repair_blocks=data['local_repair_blocks'])


def preflight(client):
    before=client.hello(attempts=3)
    if before.get('pending'):raise RuntimeError('target application is awaiting boot confirmation')
    client.request(25)
    # VERIFY is the expected refusal with no sample/session at offset zero.
    # FORMAT identifies a peer without the laboratory sample verifier.
    client.request(26,0,expected=4)
    net=client.request(NETWORK_STATUS)
    if not ALLOW_ONLINE_TARGET and (net.get('wifi_mode')!=0 or net.get('active_interface')):
        raise RuntimeError('target must have Wi-Fi and AP disabled for matched trials')
    return before


# Functional checks only: a target on Wi-Fi is not comparable with matched trials.
ALLOW_ONLINE_TARGET=False


def summarize(rows):
    groups={}
    for row in rows:groups.setdefault(row['block_bytes'],[]).append(row)
    result=[]
    for block,runs in sorted(groups.items()):
        success=[r for r in runs if r['success']]
        entry=dict(block_bytes=block,attempted=len(runs),successful=len(success))
        for field in ('duration_s','airtime_s','controller_retransmissions','relay_local_repairs'):
            values=sorted(r[field] for r in success)
            if values:entry[field]=dict(mean=statistics.mean(values),median=statistics.median(values),
                                        p95=values[math.ceil(.95*len(values))-1],maximum=max(values))
        result.append(entry)
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('mode',choices=('plan','run'))
    p.add_argument('--radio-profiles',type=Path,required=True)
    p.add_argument('--blocks',type=lambda s:[int(x) for x in s.split(',')],default=list(DEFAULT_BLOCKS))
    p.add_argument('--repeats',type=int,default=5);p.add_argument('--seed',type=int,default=20261101)
    p.add_argument('--condition',required=True,help='physical setup and attenuation/interference description')
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--via');p.add_argument('--relay-status-url')
    p.add_argument('--bridge');p.add_argument('--key-file',type=Path);p.add_argument('--route-key-file',type=Path)
    p.add_argument('--counter-file',type=Path);p.add_argument('--image',type=Path)
    p.add_argument('--sample-bytes',type=int,default=65536)
    p.add_argument('--compress',action='store_true',help='Send the sample as deflate-compressed pages')
    p.add_argument('--allow-online-target',action='store_true',help='Functional check with the target Wi-Fi on; not a matched trial')
    p.add_argument('--source',default='F4MLV-2');p.add_argument('--target',default='F4MLV-15')
    args=p.parse_args();profiles=load_profiles(args.radio_profiles)
    trials=plan(args.blocks,args.repeats,args.seed,profiles,bool(args.via))
    if args.mode=='plan':
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(dict(condition=args.condition,seed=args.seed,trials=trials,
            profiles={n:p.mapping() for n,p in profiles.items()}),indent=2)+'\n')
        print(f'{len(trials)} trials planned; no equipment accessed: {args.output}');return
    if not all((args.bridge,args.key_file,args.counter_file,args.image)):p.error('run requires bridge, key-file, counter-file, image')
    if args.via and not all((args.route_key_file,args.relay_status_url)):p.error('routed runs require route-key-file and relay-status-url')
    global ALLOW_ONLINE_TARGET;ALLOW_ONLINE_TARGET=args.allow_online_target
    if args.output.exists():raise ValueError('output directory already exists; use a new run directory')
    image=args.image.read_bytes()
    if args.sample_bytes<4096 or args.sample_bytes%PAGE or args.sample_bytes>len(image):
        raise ValueError('sample size must be whole 4 KiB pages within the supplied image')
    image=image[:args.sample_bytes];sha=hashlib.sha256(image).digest()
    key=base64.urlsafe_b64decode(args.key_file.read_text().strip()+'=')
    if len(key)!=32:raise ValueError('target key must be 32 bytes')
    args.output.mkdir(parents=True);trace=(args.output/'events.jsonl').open('w');all_rows=[];current=[]
    def event(kind,**fields):
        row=dict(time=time.time(),event=kind,**fields);trace.write(json.dumps(row)+'\n');trace.flush();current.append(row)
    manifest=dict(condition=args.condition,seed=args.seed,source=args.source,target=args.target,relay=args.via,
                  bytes=len(image),sha256=sha.hex(),trials=trials,profiles={n:p.mapping() for n,p in profiles.items()},
                  verification='inactive flash prefix SHA-256; no image validation or activation')
    (args.output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    bridge_device=igate_bridge(args.bridge);igate=bridge_device is not None
    event('controller_radio',firmware='igate' if igate else 'bridge')
    c=Client(args.bridge,key,args.source.encode(),args.target.encode(),args.counter_file,event,bridge_device)
    if args.via:
        routekey=base64.urlsafe_b64decode(args.route_key_file.read_text().strip()+'=')
        c.route=RouteTransport(routekey,c.source,c.target,args.via.encode())
    c.windowed=True;session_open=False;may_abort=False
    try:
        for trial in trials:
            row=dict(trial,success=False,condition=args.condition)
            try:
                may_abort=False
                c.block_size=trial['block_bytes'];c.set_radio_profiles(profiles);c.enable_profiles()
                # Clear a previous image checkpoint before beginning a fresh trial.
                session_open=True;preflight(c)
                if c.route:relay_stats(args.relay_status_url,dict(controller=args.source,target=args.target,via=args.via))
                may_abort=True;c.request(ABORT);session_open=False
                c.hello(attempts=3);session_open=True
                c.enter_fast(60)
                target_before=c.request(25);relay_before=relay_stats(args.relay_status_url,dict(controller=args.source,target=args.target,via=args.via)) if c.route else None
                current.clear();started=time.monotonic()
                result=transfer(c,image,sha,event,block_size=trial['block_bytes'],verify_sample=True,compress=args.compress)
                row['duration_s']=time.monotonic()-started
                target_after=c.request(25);relay_after=relay_stats(args.relay_status_url,dict(controller=args.source,target=args.target,via=args.via)) if c.route else None
                td=counter_delta(target_before,target_after,'stats_boot')
                rd=counter_delta(relay_before,relay_after,'boot_id') if c.route else dict(tx_frames=0,tx_bytes=0,tx_airtime_us=0,tx_errors=0)
                tx=[e for e in current if e['event']=='rf_tx_done' and e['op']!=25 and e['ok']]
                air=sum(profiles['transfer' if e['fast'] else 'normal'].airtime(e['wire_bytes']) for e in tx)
                data=[e for e in tx if e['op']==16];retrans=len(data)-len({e['offset'] for e in data})
                # The receiver reports transported bytes: the container when compressed.
                if not args.compress and result['written']!=len(image):raise RuntimeError('sample verification did not cover all bytes')
                row.update(success=True,target_elf_sha256=target_before.get('elf_sha256'),
                    relay_elf_sha256=relay_before.get('elf_sha256') if c.route else None,airtime_s=air+(td['tx_airtime_us']+rd['tx_airtime_us'])/1e6,
                    controller_tx_frames=len(tx),target_tx_frames=td['tx_frames'],relay_tx_frames=rd['tx_frames'],
                    controller_retransmissions=retrans,
                    relay_local_repairs=relay_after['local_repair_blocks']-relay_before['local_repair_blocks'] if c.route else 0,
                    target_counters=td,relay_counters=rd)
                c.return_normal();c.request(ABORT);session_open=False
            except Exception as error:
                row['success']=False;row['error']=str(error)
                all_rows.append(row);(args.output/'trials.json').write_text(json.dumps(all_rows,indent=2)+'\n')
                raise
            all_rows.append(row);(args.output/'trials.json').write_text(json.dumps(all_rows,indent=2)+'\n')
            print(json.dumps(row),flush=True)
    finally:
        # Best effort only: if radio contact is lost, the local leases remain
        # the recovery mechanism. The error is retained, never counted as success.
        try:
            if session_open and may_abort:
                if c.fast:c.return_normal()
                c.request(ABORT,attempts=1)
            elif session_open:
                c.bridge_profile(False);event("preflight_refused_checkpoint_preserved_session_expires")
        except Exception as error:event('cleanup_unconfirmed',error=str(error))
        # An iGate controller returns to its configured profile and normal service.
        if igate:
            try:c.serial.write(b'BRIDGE OFF\n');c.serial.flush()
            except Exception as error:event('bridge_off_unconfirmed',error=str(error))
        c.serial.close();trace.close()
        (args.output/'summary.json').write_text(json.dumps(summarize(all_rows),indent=2)+'\n')
        if all_rows:
            columns=('repetition','block_bytes','condition','success','duration_s','airtime_s',
                     'controller_retransmissions','relay_local_repairs','error')
            with (args.output/'trials.csv').open('w',newline='') as out:
                writer=csv.DictWriter(out,columns,extrasaction='ignore');writer.writeheader();writer.writerows(all_rows)

if __name__=='__main__':main()
