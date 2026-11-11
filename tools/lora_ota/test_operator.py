import base64
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch
from operator_config import Settings, validate_station
from radio_profiles import load_profiles
from send_integrated import Client, OperatorPause, WINDOW_DATA, WINDOW_ACTIVATE


def station():
    return dict(name='Test station', source='N7UV-13', target='N7UV',
        key=base64.urlsafe_b64encode(bytes(range(32))).decode().rstrip('='), via='',route_key='',
        normal=dict(frequency_hz=433775000, bandwidth_hz=125000, spreading_factor=7,coding_rate=6,power_dbm=2),
        transfer=dict(frequency_hz=434550000,bandwidth_hz=250000,spreading_factor=8,coding_rate=6,power_dbm=3))


def image(path):
    b=bytearray(512);b[0]=0xe9;b[32:36]=bytes.fromhex('3254cdab');path.write_bytes(b)


class OperatorSettingsTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.settings=Settings(self.root/'app')
        self.firmware=self.root/'firmware.bin';image(self.firmware)

    def test_roundtrip_hidden_secrets_and_private_files(self):
        saved=self.settings.put(station());loaded=Settings(self.settings.directory)
        self.assertEqual(saved,loaded.stations[0])
        args,folder,size,sha=loaded.prepare_run(saved,self.firmware,'COM4')
        self.assertNotIn(saved['key'],' '.join(args));self.assertEqual(size,512)
        self.assertEqual((folder/'target.key').read_text(),saved['key'])
        self.assertEqual(load_profiles(folder/'profiles.json')['normal'].spreading_factor,7)
        if os.name!='nt':
            self.assertEqual(self.settings.path.stat().st_mode&0o777,0o600)
            self.assertEqual((folder/'target.key').stat().st_mode&0o777,0o600)

    def test_firmware_is_snapshotted_and_counter_survives_edit_and_removal(self):
        saved=self.settings.put(station());args,folder,_,_=self.settings.prepare_run(saved,self.firmware,'COM4')
        self.firmware.write_bytes(b'changed after selection')
        self.assertEqual((folder/'firmware.bin').stat().st_size,512)
        self.assertEqual(args[args.index('--image')+1],str(folder/'firmware.bin'))
        counter=self.settings.counter_path(saved);counter.write_text('1234567890')
        edited=dict(saved,name='Renamed');self.settings.put(edited)
        self.assertEqual(self.settings.counter_path(edited),counter)
        self.settings.remove(saved['id']);again=self.settings.put(station())
        self.assertEqual(self.settings.counter_path(again).read_text(),'1234567890')

    def test_direct_and_routed_launches_have_no_fault_injection(self):
        s=self.settings.put(station());a,folder,_,_=self.settings.prepare_run(s,self.firmware,'COM1')
        self.assertNotIn('--via',a);self.assertIn('--windowed',a)
        self.assertEqual(a[a.index('--block-size')+1],'128')
        self.assertFalse(any(x.startswith('--exercise') or x=='--fault-checks' for x in a))
        routed=dict(s,via='PINAL',route_key=s['key'],require_offline=True)
        routed=self.settings.put(routed);b,nextfolder,_,_=self.settings.prepare_run(routed,self.firmware,'COM1')
        self.assertNotEqual(folder,nextfolder);self.assertEqual(b[b.index('--via')+1],'PINAL')
        self.assertIn('--require-offline',b);self.assertTrue((nextfolder/'route.key').exists())

    def test_invalid_settings_and_images_cannot_launch(self):
        for changes in ({'target':'TOO-LONG-13'},{'key':'bad'},{'via':'PINAL','route_key':''},
                        {'normal':dict(station()['normal'],spreading_factor=5)}):
            with self.subTest(changes=list(changes)):
                with self.assertRaises(ValueError):validate_station(dict(station(),**changes))
        s=self.settings.put(station())
        for offset,value in ((0,0xff),(12,5),(32,0)):
            image(self.firmware);data=bytearray(self.firmware.read_bytes());data[offset]=value;self.firmware.write_bytes(data)
            with self.assertRaises(ValueError):self.settings.prepare_run(s,self.firmware,'COM4')

    def test_duplicate_pair_and_unknown_schema_do_not_overwrite_data(self):
        self.settings.put(station());before=self.settings.path.read_bytes()
        with self.assertRaises(ValueError):self.settings.put(station())
        self.assertEqual(self.settings.path.read_bytes(),before)
        self.settings.path.write_text('{"schema":99}')
        with self.assertRaises(ValueError):Settings(self.settings.directory)
        self.assertEqual(self.settings.path.read_text(),'{"schema":99}')

    def test_pause_prevents_radio_writes_before_activation(self):
        c=Client.__new__(Client);c.pause_file=self.root/'pause';c.pause_file.write_text('pause')
        c.serial=Mock()
        with self.assertRaises(OperatorPause):c.request(WINDOW_ACTIVATE)
        with self.assertRaises(OperatorPause):c.send_unacknowledged(WINDOW_DATA,0,b'data')
        c.serial.write.assert_not_called()
        c.activation_started=True;c.check_pause()  # Finish confirmation once activation has started.
        c.activation_started=False;c.closing=True;c.check_pause()  # Permit normal-profile cleanup.


if __name__=='__main__':unittest.main()
