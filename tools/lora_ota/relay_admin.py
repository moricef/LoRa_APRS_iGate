#!/usr/bin/env python3
"""Private bench management over the relay AP, never a firmware data path to the target."""
import argparse,base64,hashlib,json,time,urllib.request,urllib.parse,threading,gzip
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('action',choices=['upload','backup','stats','route','pause','resume','restore'])
    ap.add_argument('--host',default='http://192.168.4.1');ap.add_argument('--config',type=Path,required=True)
    ap.add_argument('--file',type=Path);ap.add_argument('--key-file',type=Path);ap.add_argument('--expected-sha');ap.add_argument('--original-build')
    args=ap.parse_args();cfg=json.loads(args.config.read_text())
    if cfg.get('callsign')!='F4MLV-10':raise ValueError('unexpected relay config')
    web=cfg.get('webadmin',{});ota=cfg.get('ota',{})
    if web.get('active'):user,pw=web.get('username',''),web.get('password','')
    elif ota.get('username') and ota.get('password'):user,pw=ota['username'],ota['password']
    else:user,pw='ota-lab',cfg['wifi']['autoAP']['password']
    def req(path,data=None,content=None,timeout=15):
        if isinstance(data,dict):data=urllib.parse.urlencode(data).encode();content='application/x-www-form-urlencoded'
        r=urllib.request.Request(args.host+path,data=data)
        r.add_header('Authorization','Basic '+base64.b64encode((user+':'+pw).encode()).decode())
        if content:r.add_header('Content-Type',content)
        with urllib.request.urlopen(r,timeout=timeout) as f:return f.status,f.read()
    if args.action=='upload':
        b=args.file.read_bytes();sha=hashlib.sha256(b).hexdigest()
        if b[:1]!=b'\xe9' or not args.expected_sha or sha!=args.expected_sha:raise ValueError('image mismatch')
        code,body=req('/ota/start?mode=firmware&hash='+hashlib.md5(b).hexdigest())
        if code!=200:raise RuntimeError('OTA start rejected')
        boundary='----LoRaOtaBench20261030'
        data=('--'+boundary+'\r\nContent-Disposition: form-data; name="file"; filename="firmware.bin"\r\nContent-Type: application/octet-stream\r\n\r\n').encode()+b+('\r\n--'+boundary+'--\r\n').encode()
        outcome={}
        def upload():
            try:
                code,body=req('/ota/upload',data,'multipart/form-data; boundary='+boundary,timeout=240)
                outcome.update(status=code,response=body.decode(errors='replace'))
            except Exception as error:outcome['response_error']=type(error).__name__
        threading.Thread(target=upload,daemon=True).start()
        wanted_elf=b[176:208].hex();deadline=time.monotonic()+300
        while time.monotonic()<deadline:
            time.sleep(3)
            if outcome.get('status',200)!=200:raise RuntimeError('OTA upload rejected')
            try:
                if args.original_build:
                    _,html=req('/',timeout=3)
                    if html[:2]==b'\x1f\x8b':html=gzip.decompress(html)
                    verified=args.original_build.encode() in html
                else:
                    _,body=req('/lab/ota/route',timeout=3)
                    verified=json.loads(body).get('elf_sha256')==wanted_elf
                if verified:
                    print(json.dumps(dict(bytes=len(b),sha256=sha,running_verified=True,**outcome)),flush=True);return
            except Exception:pass
        raise TimeoutError('Running firmware not verified after upload')
    elif args.action=='backup':
        code,body=req('/lab/ota/config-backup');r=json.loads(body)
        if r.get('callsign')!='F4MLV-10':raise ValueError('wrong device')
        args.file.write_bytes(body);args.file.chmod(0o600);print('Stored original configuration file',len(body),'bytes')
    elif args.action=='stats':
        code,body=req('/lab/ota/route');print(body.decode())
    elif args.action=='route':
        code,body=req('/lab/ota/route',{'controller':'F4MLV-2','target':'F4MLV-15','key':args.key_file.read_text().strip()});print(code,body.decode())
    elif args.action in ('pause','resume'):
        code,body=req('/lab/ota/pause',{'paused':'1' if args.action=='pause' else '0'});print(code,body.decode())
    elif args.action=='restore':
        code,body=req('/lab/ota/config-restore',{'json':args.file.read_text()});print(code,body.decode())

if __name__=='__main__':main()
