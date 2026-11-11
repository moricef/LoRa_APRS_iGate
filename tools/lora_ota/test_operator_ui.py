"""UI integration with a subprocess simulator. Never opens a serial port."""
import json
from pathlib import Path
import sys
import tempfile
import time
import tkinter as tk
import unittest
from unittest.mock import patch
from operator_config import Settings
from operator_ui import OperatorApp, StationDialog
from test_operator import station, image


class InterfaceTests(unittest.TestCase):
    def setUp(self):
        try:
            root=tk.Tk();root.withdraw();root.destroy()
        except tk.TclError as error:self.skipTest(str(error))
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.settings=Settings(self.root/'app')
        self.settings.put(station());self.firmware=self.root/'firmware.bin';image(self.firmware)
        self.app=None

    def tearDown(self):
        if self.app:
            if self.app.process:
                self.app.process.terminate();self.app.process.wait(timeout=3)
            try:self.app.destroy()
            except tk.TclError:pass

    def app_for(self,code):
        wrapped="import sys;sys.stdout=open(sys.argv[sys.argv.index('--log')+1],'w',buffering=1);\n"+code
        self.app=OperatorApp(self.settings,lambda args:[sys.executable,'-u','-c',wrapped,*args])
        self.app.withdraw();self.app.port.set('SIMULATED-NO-RADIO')
        self.app.firmware.set(str(self.firmware));return self.app

    def wait(self,predicate,seconds=4):
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            self.app.update()
            if predicate():return
            time.sleep(.02)
        self.fail('UI timed out')

    def test_success_requires_new_boot_confirmation(self):
        code="import json;print(json.dumps({'event':'page_committed','written':512,'total':512}));print(json.dumps({'event':'radio_confirmed'}))"
        app=self.app_for(code)
        with patch('serial.Serial',side_effect=AssertionError('No radio allowed')):
            app.start();self.wait(lambda:app.process is None)
        self.assertEqual(app.progress.get(),100)
        self.assertIn('updated and confirmed',app.phase.get())
        self.assertFalse((app.run_directory/'target.key').exists())

    def test_clean_process_exit_without_confirmation_is_failure(self):
        app=self.app_for("print('{\"event\":\"verified\"}')")
        app.start();self.wait(lambda:app.process is None)
        self.assertIn('failed',app.phase.get());self.assertNotEqual(app.progress.get(),100)

    def test_pause_is_a_request_and_preserves_counter_location(self):
        code="""import sys,time,json
from pathlib import Path
p=Path(sys.argv[sys.argv.index('--pause-file')+1])
while not p.exists():time.sleep(.01)
print(json.dumps({'event':'operator_paused'}))
raise SystemExit(75)
"""
        app=self.app_for(code);app.start();app.pause()
        self.assertTrue((app.run_directory/'pause.request').exists())
        self.wait(lambda:app.process is None)
        self.assertTrue(app.phase.get().startswith('Paused.'))
        self.assertNotEqual(app.progress.get(),100)

    def test_pause_is_disabled_during_activation_and_confirmation(self):
        app=self.app_for('pass');app.handle_event({'event':'activation_starting'})
        self.assertTrue(app.confirming)
        self.assertIn('disabled',app.pause_button.state())
        self.assertFalse(app.got_confirmation)
        app.handle_event({'event':'radio_confirmed'});self.assertTrue(app.got_confirmation)

    def test_both_languages_and_masked_secret_fields(self):
        app=self.app_for('pass');app.change_language('Français')
        self.assertEqual(app.update_button['text'],'Mettre à jour')
        d=StationDialog(app,self.settings.stations[0]);d.withdraw();d.update_idletasks()
        def entries(w):
            for c in w.winfo_children():
                if c.winfo_class()=='TEntry':yield c
                yield from entries(c)
        hidden=[e for e in entries(d) if str(e.cget('show'))]
        self.assertEqual(len(hidden),2)
        self.assertEqual(d.fields['key'].get(),self.settings.stations[0]['key'])
        d.destroy();app.change_language('English')
        self.assertEqual(app.update_button['text'],'Update')


if __name__=='__main__':unittest.main()
