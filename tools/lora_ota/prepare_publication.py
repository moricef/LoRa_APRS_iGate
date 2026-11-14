#!/usr/bin/env python3
"""Prepare the three OTA flasher builds and a secret-free client bundle."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
BOARDS = (
    ('ttgo-lora32-v21-ota', 'ttgo-lora32-v21_SD', 'manifest-ttgo-lora32-v21-sd.json'),
    ('heltec_wifi_lora_32_V3_2-ota', 'heltec_wifi_lora_32_V3_2', 'manifest-heltec-wifi-lora-32-v3-2.json'),
    ('QRPLabs_LightGateway_Plus_1_0-ota', 'QRPLabs_LightGateway_Plus_1_0', 'manifest-qrplabs-lightgateway-plus-1-0.json'),
)

def main():
    docs = ROOT / 'docs'
    boot_app = Path.home() / '.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'
    metadata = []
    for env, folder, manifest_name in BOARDS:
        build = ROOT / '.pio/build' / env
        binary = (build / 'firmware.bin').read_bytes()
        info = []
        for match in re.finditer(b'\x1f\x8b\x08', binary):
            try:
                content = zlib.decompress(binary[match.start():], 31)
            except zlib.error:
                continue
            found = re.search(rb'Firmware: ([^<]+).*?Build date: ([^<]+)', content)
            if found:
                info.append(tuple(x.decode('ascii') for x in found.groups()))
        if len(info) != 1 or info[0][0] != '4.0.2RXT-OTA':
            raise RuntimeError(f'{env}: missing or unexpected embedded WebUI metadata: {info}')
        version, date = info[0]
        destination = docs / 'firmware' / folder
        destination.mkdir(parents=True, exist_ok=True)
        names = ('bootloader.bin', 'partitions.bin', 'boot_app0.bin', 'firmware.bin')
        for name in names:
            shutil.copyfile(boot_app if name == 'boot_app0.bin' else build / name, destination / name)
        sums = ''.join(f'{hashlib.sha256((destination/name).read_bytes()).hexdigest()}  {name}\n' for name in names)
        (destination / 'SHA256SUMS').write_text(sums)
        path = docs / manifest_name
        manifest = json.loads(path.read_text())
        manifest['name'] = manifest['name'].replace('(RXT)', '(RXT + LoRa OTA)')
        manifest['version'] = f'{version} ({date}, unified configurable OTA controller/relay/target, RXT v2, durable resume, authenticated activation and confirmation)'
        path.write_text(json.dumps(manifest, indent=2) + '\n')
        metadata.append(dict(environment=env, directory=folder, version=version, build_date=date,
                             bytes=len(binary), sha256=hashlib.sha256(binary).hexdigest()))
    (docs / 'firmware/OTA_BUILDS.json').write_text(json.dumps(metadata, indent=2) + '\n')
    # Only this explicit allowlist enters the downloadable archive.
    files = ('send_integrated.py', 'send.py', 'route_transport.py', 'radio_profiles.py', 'window_transfer.py', 'README.md')
    archive = docs / 'firmware/lora-ota-client.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as bundle:
        for name in files:
            bundle.write(ROOT / 'tools/lora_ota' / name, 'lora-ota-client/' + name)
        bundle.write(ROOT / 'LICENSE', 'lora-ota-client/LICENSE')
    (archive.with_suffix('.zip.sha256')).write_text(hashlib.sha256(archive.read_bytes()).hexdigest() + '  ' + archive.name + '\n')
    print(json.dumps(metadata, indent=2))

if __name__ == '__main__':
    main()
