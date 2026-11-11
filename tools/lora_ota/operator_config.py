"""Persistent operator settings; protocol counters and temporary key files stay private."""
from __future__ import annotations
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import uuid
from radio_profiles import RadioProfile


def default_directory():
    if sys.platform == 'win32':
        return Path(os.environ.get('LOCALAPPDATA', Path.home() / 'AppData/Local')) / 'LoRaFirmwareUpdate'
    return Path(os.environ.get('XDG_CONFIG_HOME', Path.home() / '.config')) / 'lora-firmware-update'


def private_write(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    fd, temporary = tempfile.mkstemp(prefix='.' + path.name, dir=path.parent)
    try:
        os.chmod(temporary, 0o600)
        with os.fdopen(fd, 'wb' if isinstance(data, bytes) else 'w', **({} if isinstance(data, bytes) else {'encoding': 'utf-8'})) as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def identity(text, label):
    value = text.strip().upper()
    if not re.fullmatch(r'[A-Z0-9][A-Z0-9-]{0,7}', value):
        raise ValueError(label + ': use 1 to 8 letters, digits or hyphens.')
    return value


def secret(text, label):
    value = text.strip()
    if not re.fullmatch(r'[A-Za-z0-9_-]{43}', value):
        raise ValueError(label + ': paste the 43-character secret configured on the device.')
    if len(base64.urlsafe_b64decode(value + '=')) != 32:
        raise ValueError(label + ': invalid secret.')
    return value


def validate_station(station):
    result = dict(station)
    result['id'] = str(uuid.UUID(result.get('id', str(uuid.uuid4()))))
    result['source'] = identity(result.get('source', ''), 'Controller')
    result['target'] = identity(result.get('target', ''), 'Station identity')
    result['name'] = result.get('name', '').strip() or result['target']
    result['key'] = secret(result.get('key', ''), 'Station secret')
    result['via'] = identity(result['via'], 'Relay') if result.get('via', '').strip() else ''
    result['route_key'] = secret(result.get('route_key', ''), 'Route secret') if result['via'] else ''
    result['require_offline'] = bool(result.get('require_offline', False))
    for name in ('normal', 'transfer'):
        p = RadioProfile.from_mapping(result[name])
        # The supplied controller and BOARD identifier have been validated on SX1278 TTGO only.
        if p.spreading_factor < 7 or p.power_dbm > 20:
            raise ValueError(name + ': this SX1278 controller supports SF7–SF12 and at most 20 dBm.')
        result[name] = p.mapping()
    return result


class Settings:
    def __init__(self, directory=None):
        self.directory = Path(directory) if directory else default_directory()
        self.path = self.directory / 'settings.json'
        self.language = 'en'
        self.port = ''
        self.stations = []
        if self.path.exists():
            data = json.loads(self.path.read_text(encoding='utf-8'))
            if data.get('schema') != 1:
                raise ValueError('Unsupported settings version; your file has not been changed.')
            self.language = data.get('language', 'en')
            self.port = data.get('port', '')
            self.stations = [validate_station(s) for s in data.get('stations', [])]

    def save(self):
        self.directory.mkdir(parents=True, exist_ok=True, mode=0o700)
        os.chmod(self.directory, 0o700)
        private_write(self.path, json.dumps(dict(schema=1, language=self.language,
                    port=self.port, stations=self.stations), indent=2))

    def put(self, station):
        value = validate_station(station)
        old = next((i for i, s in enumerate(self.stations) if s['id'] == value['id']), None)
        if any(s['id'] != value['id'] and s['source'] == value['source'] and
               s['target'] == value['target'] for s in self.stations):
            raise ValueError('This controller/station pair already has a saved configuration.')
        if old is None:
            self.stations.append(value)
        else:
            self.stations[old] = value
        self.save()
        return value

    def remove(self, station_id):
        self.stations = [s for s in self.stations if s['id'] != station_id]
        self.save()  # Deliberately retain counters even when a station is removed.

    def counter_path(self, station):
        pair = station['source'] + '\0' + station['target']
        return self.directory / 'counters' / (hashlib.sha256(pair.encode()).hexdigest() + '.counter')

    def prepare_run(self, station, firmware, port):
        station = validate_station(station)
        firmware = Path(firmware).resolve()
        if not port.strip():
            raise ValueError('Select the USB radio bridge.')
        data = firmware.read_bytes()
        if (len(data) < 256 or data[:1] != b'\xe9' or len(data) > 1966080 or
                int.from_bytes(data[12:14], 'little') != 0 or data[32:36] != bytes.fromhex('3254cdab')):
            raise ValueError('Select an ESP32 application firmware.bin for the compatible TTGO LoRa32 V2.1.')
        # Each attempt has a unique directory. Resuming never overwrites an earlier log.
        run = self.directory / 'transfers' / str(uuid.uuid4())
        run.mkdir(parents=True, mode=0o700)
        private_write(run / 'firmware.bin', data)
        private_write(run / 'target.key', station['key'])
        private_write(run / 'profiles.json', json.dumps({k: station[k] for k in ('normal', 'transfer')}))
        counter = self.counter_path(station)
        counter.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        args = ['--bridge', port, '--source', station['source'], '--target', station['target'],
                '--key-file', str(run / 'target.key'), '--counter-file', str(counter),
                '--image', str(run / 'firmware.bin'), '--pause-file', str(run / 'pause.request'), '--log', str(run / 'transfer.jsonl'),
                '--windowed', '--block-size', '128', '--negotiate-profile',
                '--radio-profiles', str(run / 'profiles.json')]
        if station['via']:
            private_write(run / 'route.key', station['route_key'])
            args += ['--via', station['via'], '--route-key-file', str(run / 'route.key')]
        if station['require_offline']:
            args += ['--require-offline']
        self.port = port
        self.save()
        return args, run, len(data), hashlib.sha256(data).hexdigest()
