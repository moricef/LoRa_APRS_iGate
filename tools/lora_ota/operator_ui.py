#!/usr/bin/env python3
"""Desktop operator interface for the existing RF firmware update client."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
import webbrowser
from serial.tools import list_ports
from operator_config import Settings, validate_station

FR = {
 'LoRa Firmware Update': 'Mise à jour firmware LoRa',
 'Radio bridge': 'Pont radio', 'Refresh': 'Actualiser', 'Station': 'Station',
 'Add station': 'Ajouter une station', 'Settings': 'Réglages',
 'Firmware': 'Firmware', 'Choose file…': 'Choisir le fichier…',
 'Update': 'Mettre à jour', 'Pause': 'Mettre en pause', 'Ready': 'Prêt',
 'Saved settings are used automatically.': 'Les réglages enregistrés sont utilisés automatiquement.',
 'The update includes verification, activation and confirmation of the new firmware.':
 'La mise à jour comprend la vérification, l’activation et la confirmation du nouveau firmware.',
 'Choose an OTA-compatible application firmware for the target.':
 'Choisir un firmware applicatif compatible OTA pour la cible.',
 'No station configured': 'Aucune station configurée', 'No USB radio found': 'Aucun pont radio USB détecté',
 'Open transfer folder': 'Ouvrir le dossier du transfert',
 'Contacting station…': 'Connexion à la station…', 'Preparing radio link…': 'Préparation de la liaison radio…',
 'Sending firmware…': 'Envoi du firmware…', 'Verifying received firmware…': 'Vérification du firmware reçu…',
 'Starting new firmware…': 'Démarrage du nouveau firmware…',
 'Confirming new firmware…': 'Confirmation du nouveau firmware…',
 'Firmware updated and confirmed.': 'Firmware mis à jour et confirmé.',
 'Transfer failed. Open the transfer folder for details.': 'Échec du transfert. Les détails sont dans son dossier.',
 'Paused. Allow up to one minute for the radio link to return to normal, then restart with the same file.':
 'En pause. Attendre au plus une minute le retour radio normal, puis relancer avec le même fichier.',
 'Link interrupted; recovering…': 'Liaison interrompue ; récupération…',
 'Repairing missing blocks…': 'Renvoi des blocs manquants…',
 'Station settings': 'Réglages de la station', 'Name': 'Nom',
 'Controller callsign': 'Indicatif du contrôleur', 'Station identity': 'Identité de la station',
 'Station secret': 'Clé de la station', 'Use a relay': 'Passer par un relais',
 'Relay identity': 'Identité du relais', 'Shared route secret': 'Clé commune de la route',
 'Normal radio profile': 'Profil radio habituel', 'Transfer radio profile': 'Profil radio du transfert',
 'Frequency (MHz)': 'Fréquence (MHz)', 'Bandwidth (kHz)': 'Bande passante (kHz)',
 'Spreading factor': 'Facteur d’étalement', 'Coding rate': 'Taux de codage', 'Power (dBm)': 'Puissance (dBm)',
 'Import radio profiles…': 'Importer les profils radio…', 'Save': 'Enregistrer', 'Cancel': 'Annuler',
 'Check settings': 'Vérifier les réglages', 'Update cannot start': 'La mise à jour ne peut pas démarrer',
 'Choose a station and a firmware file first.': 'Choisir d’abord une station et un fichier firmware.',
 'The station and any relay must already have compatible firmware and matching keys and route settings.':
 'La station et son relais éventuel doivent déjà avoir un firmware compatible, les mêmes clés et la route configurée.',
 'Use the station Callsign-SSID, not its tactical name.':
 'Utiliser le champ Callsign-SSID de la station, pas son nom tactique.',
 'Require station Wi-Fi to be off': 'Exiger le Wi-Fi de la station désactivé',
 'The normal profile must match the station; the transfer profile is temporary.':
 'Le profil habituel doit correspondre à la station ; le profil de transfert est temporaire.',
 'Transfer in progress': 'Transfert en cours',
 'Pausing after the current radio exchange…': 'Mise en pause après l’échange radio en cours…',
 'Pause this transfer and close?': 'Mettre ce transfert en pause et fermer ?',
 'Boot confirmation is in progress. Please wait for completion.':
 'La confirmation du démarrage est en cours. Attendre sa fin.',
 'Language': 'Langue', 'Invalid settings file': 'Fichier de réglages invalide',
}

def text(language, phrase):
    return FR.get(phrase, phrase) if language == 'fr' else phrase


def engine_command(args):
    output = str(Path(args[args.index('--log') + 1]).with_name('client-output.log'))
    prefix = [sys.executable] if getattr(sys, 'frozen', False) else [sys.executable, str(Path(__file__))]
    return [*prefix, '--ota-engine', '--operator-output', output, *args]


class StationDialog(tk.Toplevel):
    def __init__(self, app, station=None):
        super().__init__(app)
        self.app = app
        self.t = app.t
        self.result = None
        self.original = station or {}
        self.title(self.t('Station settings'))
        self.transient(app)
        self.resizable(False, False)
        body = ttk.Frame(self, padding=18)
        body.grid(sticky='nsew')
        self.fields = {}
        for row, (key, label) in enumerate((('name', 'Name'), ('source', 'Controller callsign'),
                         ('target', 'Station identity'), ('key', 'Station secret'))):
            ttk.Label(body, text=self.t(label)).grid(row=row, column=0, sticky='w', pady=4)
            var = tk.StringVar(value=self.original.get(key, ''))
            self.fields[key] = var
            ttk.Entry(body, textvariable=var, width=46, show='•' if key == 'key' else '').grid(row=row, column=1, columnspan=2, sticky='ew')
        ttk.Label(body, text=self.t('Use the station Callsign-SSID, not its tactical name.'),
                  wraplength=610).grid(row=4, column=0, columnspan=3, sticky='w', pady=(3, 10))
        self.use_relay = tk.BooleanVar(value=bool(self.original.get('via')))
        ttk.Checkbutton(body, text=self.t('Use a relay'), variable=self.use_relay,
                        command=self.route_state).grid(row=5, column=0, columnspan=3, sticky='w')
        self.route_entries = []
        for row, (key, label) in enumerate((('via', 'Relay identity'), ('route_key', 'Shared route secret')), start=6):
            ttk.Label(body, text=self.t(label)).grid(row=row, column=0, sticky='w', pady=4)
            var = tk.StringVar(value=self.original.get(key, ''))
            self.fields[key] = var
            entry = ttk.Entry(body, textvariable=var, width=46, show='•' if key == 'route_key' else '')
            entry.grid(row=row, column=1, columnspan=2, sticky='ew')
            self.route_entries.append(entry)
        self.profiles = {}
        for col, name, label in ((0, 'normal', 'Normal radio profile'), (1, 'transfer', 'Transfer radio profile')):
            frame = ttk.LabelFrame(body, text=self.t(label), padding=10)
            frame.grid(row=8, column=col, columnspan=1 if col == 0 else 2, sticky='nsew', padx=(0, 8) if col == 0 else (8, 0), pady=10)
            current = self.original.get(name, {})
            # Frequency is deliberately blank on a new station: no guessed remote configuration.
            values = {'frequency_hz': str(current['frequency_hz'] / 1e6) if 'frequency_hz' in current else '',
                      'bandwidth_hz': str(current.get('bandwidth_hz', 125000 if name == 'normal' else 250000) // 1000),
                      'spreading_factor': str(current.get('spreading_factor', 7 if name == 'normal' else 8)),
                      'coding_rate': '4/' + str(current.get('coding_rate', 6)),
                      'power_dbm': str(current.get('power_dbm', 2 if name == 'normal' else 3))}
            self.profiles[name] = {}
            for row, (key, title) in enumerate((('frequency_hz', 'Frequency (MHz)'), ('bandwidth_hz', 'Bandwidth (kHz)'),
                  ('spreading_factor', 'Spreading factor'), ('coding_rate', 'Coding rate'), ('power_dbm', 'Power (dBm)'))):
                ttk.Label(frame, text=self.t(title)).grid(row=row, column=0, sticky='w', pady=3)
                var = tk.StringVar(value=values[key])
                self.profiles[name][key] = var
                options = {'bandwidth_hz': ('125', '250', '500'), 'spreading_factor': tuple(map(str, range(7, 13))),
                           'coding_rate': ('4/5', '4/6', '4/7', '4/8')}.get(key)
                entry = ttk.Combobox(frame, textvariable=var, values=options, state='readonly', width=10) if options else ttk.Entry(frame, textvariable=var, width=12)
                entry.grid(row=row, column=1, padx=(10, 0))
        ttk.Label(body, text=self.t('The normal profile must match the station; the transfer profile is temporary.'),
                  wraplength=630).grid(row=9, column=0, columnspan=3, sticky='w')
        ttk.Button(body, text=self.t('Import radio profiles…'), command=self.import_profiles).grid(row=10, column=0, columnspan=3, sticky='w', pady=8)
        self.offline = tk.BooleanVar(value=self.original.get('require_offline', False))
        ttk.Checkbutton(body, text=self.t('Require station Wi-Fi to be off'), variable=self.offline).grid(row=11, column=0, columnspan=3, sticky='w')
        ttk.Label(body, text=self.t('The station and any relay must already have compatible firmware and matching keys and route settings.'),
                  wraplength=630).grid(row=12, column=0, columnspan=3, pady=12, sticky='w')
        buttons = ttk.Frame(body)
        buttons.grid(row=13, column=0, columnspan=3, sticky='e')
        ttk.Button(buttons, text=self.t('Cancel'), command=self.destroy).pack(side='left', padx=5)
        ttk.Button(buttons, text=self.t('Save'), command=self.save).pack(side='left')
        self.route_state()
        self.grab_set()

    def route_state(self):
        for entry in self.route_entries:
            entry.configure(state='normal' if self.use_relay.get() else 'disabled')

    def import_profiles(self):
        name = filedialog.askopenfilename(parent=self, filetypes=[('Radio profiles', '*.json')])
        if not name:
            return
        try:
            data = json.loads(Path(name).read_text())
            for name in ('normal', 'transfer'):
                for key, var in self.profiles[name].items():
                    value = data[name][key]
                    if key == 'frequency_hz': value /= 1e6
                    if key == 'bandwidth_hz': value //= 1000
                    if key == 'coding_rate': value = '4/' + str(value)
                    var.set(str(value))
        except (OSError, ValueError, KeyError) as error:
            messagebox.showerror(self.t('Check settings'), str(error), parent=self)

    def save(self):
        try:
            station = {k: v.get() for k, v in self.fields.items()}
            if self.original.get('id'): station['id'] = self.original['id']
            if not self.use_relay.get(): station['via'] = station['route_key'] = ''
            for name, fields in self.profiles.items():
                values = {k: v.get().strip() for k, v in fields.items()}
                station[name] = {'frequency_hz': int(round(float(values['frequency_hz'].replace(',', '.')) * 1e6)),
                                 'bandwidth_hz': int(values['bandwidth_hz']) * 1000,
                                 'spreading_factor': int(values['spreading_factor']),
                                 'coding_rate': int(values['coding_rate'].split('/')[-1]),
                                 'power_dbm': int(values['power_dbm'])}
            station['require_offline'] = self.offline.get()
            self.result = self.app.settings.put(station)
        except (ValueError, OSError, KeyError) as error:
            messagebox.showerror(self.t('Check settings'), str(error), parent=self)
            return
        self.destroy()


class OperatorApp(tk.Tk):
    def __init__(self, settings=None, command_builder=engine_command):
        super().__init__()
        self.settings = settings or Settings()
        self.command_builder = command_builder
        self.language = self.settings.language
        self.events = queue.Queue()
        self.process = None
        self.paused = False
        self.got_confirmation = False
        self.close_when_done = False
        self.confirming = False
        self.run_directory = None
        self.total = 0
        self.port = tk.StringVar(value=self.settings.port)
        self.station = tk.StringVar()
        self.firmware = tk.StringVar()
        self.phase = tk.StringVar()
        self.progress = tk.DoubleVar(value=0)
        self.detail = tk.StringVar()
        self.protocol('WM_DELETE_WINDOW', self.close)
        self.build()
        self.poll_job = self.after(100, self.poll)

    def t(self, phrase): return text(self.language, phrase)

    def build(self):
        for child in self.winfo_children(): child.destroy()
        self.title(self.t('LoRa Firmware Update'))
        self.minsize(730, 440)
        frame = ttk.Frame(self, padding=24)
        frame.pack(fill='both', expand=True)
        frame.columnconfigure(1, weight=1)
        ttk.Label(frame, text=self.t('LoRa Firmware Update'), font=('TkDefaultFont', 19, 'bold')).grid(row=0, column=0, columnspan=3, sticky='w')
        language = ttk.Combobox(frame, values=('English', 'Français'), state='readonly', width=10)
        language.set('Français' if self.language == 'fr' else 'English')
        language.grid(row=0, column=3, sticky='e')
        language.bind('<<ComboboxSelected>>', lambda e: self.change_language(language.get()))
        self.language_widget = language
        ttk.Label(frame, text=self.t('Saved settings are used automatically.')).grid(row=1, column=0, columnspan=4, sticky='w', pady=(7, 22))
        ttk.Label(frame, text=self.t('Radio bridge')).grid(row=2, column=0, sticky='w', padx=(0, 15))
        self.port_box = ttk.Combobox(frame, textvariable=self.port, width=40)
        self.port_box.grid(row=2, column=1, columnspan=2, sticky='ew', pady=6)
        self.refresh_button = ttk.Button(frame, text=self.t('Refresh'), command=self.refresh_ports)
        self.refresh_button.grid(row=2, column=3, padx=(8, 0))
        ttk.Label(frame, text=self.t('Station')).grid(row=3, column=0, sticky='w')
        self.station_box = ttk.Combobox(frame, textvariable=self.station, state='readonly')
        self.station_box.grid(row=3, column=1, sticky='ew', pady=6)
        self.settings_button = ttk.Button(frame, text=self.t('Settings'), command=self.edit_station)
        self.settings_button.grid(row=3, column=2, padx=8)
        self.add_button = ttk.Button(frame, text=self.t('Add station'), command=lambda: self.edit_station(new=True))
        self.add_button.grid(row=3, column=3)
        ttk.Label(frame, text=self.t('Firmware')).grid(row=4, column=0, sticky='w')
        ttk.Entry(frame, textvariable=self.firmware, state='readonly').grid(row=4, column=1, columnspan=2, sticky='ew', pady=6)
        self.choose_button = ttk.Button(frame, text=self.t('Choose file…'), command=self.choose_file)
        self.choose_button.grid(row=4, column=3, padx=(8, 0))
        ttk.Label(frame, text=self.t('Choose an OTA-compatible application firmware for the target.'),
                  wraplength=660).grid(row=5, column=0, columnspan=4, sticky='w', pady=(8, 20))
        self.progress_bar = ttk.Progressbar(frame, variable=self.progress, maximum=100)
        self.progress_bar.grid(row=6, column=0, columnspan=4, sticky='ew')
        ttk.Label(frame, textvariable=self.phase, font=('TkDefaultFont', 11, 'bold'), wraplength=670).grid(row=7, column=0, columnspan=4, sticky='w', pady=(10, 3))
        ttk.Label(frame, textvariable=self.detail).grid(row=8, column=0, columnspan=4, sticky='w')
        actions = ttk.Frame(frame)
        actions.grid(row=9, column=0, columnspan=4, sticky='e', pady=18)
        self.folder_button = ttk.Button(actions, text=self.t('Open transfer folder'), command=self.open_folder, state='normal' if self.run_directory else 'disabled')
        self.folder_button.pack(side='left', padx=6)
        self.pause_button = ttk.Button(actions, text=self.t('Pause'), command=self.pause, state='disabled')
        self.pause_button.pack(side='left', padx=6)
        self.update_button = ttk.Button(actions, text=self.t('Update'), command=self.start)
        self.update_button.pack(side='left', padx=6)
        ttk.Label(frame, text=self.t('The update includes verification, activation and confirmation of the new firmware.'),
                  wraplength=680).grid(row=10, column=0, columnspan=4, sticky='w')
        self.reload_stations()
        self.refresh_ports()
        if not self.phase.get(): self.phase.set(self.t('Ready'))

    def change_language(self, value):
        if self.process: return
        self.language = 'fr' if value == 'Français' else 'en'
        self.settings.language = self.language
        self.settings.save()
        self.phase.set('')
        self.build()

    def reload_stations(self, select=None):
        values = [s['name'] + ' — ' + s['target'] for s in self.settings.stations]
        self.station_box.configure(values=values)
        if select:
            i = next(i for i, s in enumerate(self.settings.stations) if s['id'] == select['id'])
            self.station.set(values[i])
        elif self.station.get() not in values:
            self.station.set(values[0] if values else '')

    def selected_station(self):
        index = self.station_box.current()
        return self.settings.stations[index] if index >= 0 else None

    def refresh_ports(self):
        ports = sorted(p.device for p in list_ports.comports())
        self.port_box.configure(values=ports)
        if not self.port.get() and len(ports) == 1: self.port.set(ports[0])

    def edit_station(self, new=False):
        dialog = StationDialog(self, None if new else self.selected_station())
        self.wait_window(dialog)
        self.reload_stations(dialog.result)

    def choose_file(self):
        name = filedialog.askopenfilename(parent=self, filetypes=[('ESP32 application', '*.bin')])
        if name: self.firmware.set(name)

    def busy(self, value):
        for widget in (self.update_button, self.add_button, self.settings_button,
                       self.choose_button, self.refresh_button, self.language_widget):
            widget.configure(state='disabled' if value else 'normal')
        self.language_widget.configure(state='disabled' if value else 'readonly')
        self.station_box.configure(state='disabled' if value else 'readonly')
        self.port_box.configure(state='disabled' if value else 'normal')
        self.pause_button.configure(state='normal' if value and not self.confirming else 'disabled')

    def start(self):
        if self.process: return
        station = self.selected_station()
        if not station or not self.firmware.get():
            messagebox.showerror(self.t('Update cannot start'), self.t('Choose a station and a firmware file first.'), parent=self)
            return
        try:
            args, directory, size, sha = self.settings.prepare_run(station, self.firmware.get(), self.port.get())
            self.run_directory, self.total = directory, size
            command = self.command_builder(args)
            flags = subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0
            self.process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=flags)
        except (OSError, ValueError) as error:
            messagebox.showerror(self.t('Update cannot start'), str(error), parent=self)
            return
        self.paused = self.confirming = self.got_confirmation = False
        self.progress.set(0)
        self.phase.set(self.t('Contacting station…'))
        self.detail.set(station['target'] + (' ← ' + station['via'] if station['via'] else ''))
        self.folder_button.configure(state='normal')
        self.busy(True)
        threading.Thread(target=self.read_process, args=(self.process, directory), daemon=True).start()

    def read_process(self, process, directory):
        # Read the engine's durable event log, including in a Windows GUI build
        # whose standard streams may be unavailable.
        import time
        stream = None
        try:
            while True:
                if stream is None and (directory / 'transfer.jsonl').exists():
                    stream = (directory / 'transfer.jsonl').open(encoding='utf-8')
                if stream:
                    while True:
                        offset = stream.tell()
                        line = stream.readline()
                        if not line.endswith('\n'):
                            stream.seek(offset)
                            break
                        try: self.events.put(('event', json.loads(line)))
                        except ValueError: pass
                result = process.poll()
                if result is not None:
                    # One more pass drains events flushed immediately before exit.
                    if stream is None and (directory / 'transfer.jsonl').exists():
                        stream = (directory / 'transfer.jsonl').open(encoding='utf-8')
                    if stream:
                        for line in stream:
                            try: self.events.put(('event', json.loads(line)))
                            except ValueError: pass
                    self.events.put(('exit', result))
                    return
                time.sleep(.05)
        finally:
            if stream: stream.close()

    def handle_event(self, row):
        kind = row.get('event')
        phases = {'radio_profiles_configured': 'Contacting station…', 'hello': 'Preparing radio link…',
                  'image': 'Sending firmware…', 'durable_progress': 'Sending firmware…',
                  'page_committed': 'Sending firmware…', 'repair_page': 'Repairing missing blocks…',
                  'link_lost': 'Link interrupted; recovering…', 'link_recovered': 'Sending firmware…',
                  'activation_starting': 'Starting new firmware…', 'verified': 'Starting new firmware…', 'normal_profile_confirmed': 'Starting new firmware…',
                  'boot_pending_confirmation': 'Confirming new firmware…'}
        if kind in phases: self.phase.set(self.t(phases[kind]))
        if kind == 'image': self.total = row['bytes']
        if kind in ('durable_progress', 'page_committed'):
            written, total = row['written'], row.get('total', self.total)
            self.progress.set(min(99, written * 100 / total) if total else 0)
            self.detail.set(f'{written / 1024:.0f} / {total / 1024:.0f} KiB')
            if written == total:
                self.phase.set(self.t('Verifying received firmware…'))
        if kind in ('activation_starting', 'boot_pending_confirmation'):
            self.confirming = True
            self.pause_button.configure(state='disabled')
        if kind == 'radio_confirmed':
            self.confirming = True
            self.got_confirmation = True

    def poll(self):
        try:
            while True:
                kind, data = self.events.get_nowait()
                if kind == 'event': self.handle_event(data)
                else:
                    self.process = None
                    self.confirming = False
                    self.busy(False)
                    if data == 0 and self.got_confirmation:
                        self.progress.set(100)
                        self.phase.set(self.t('Firmware updated and confirmed.'))
                    elif data == 75:
                        self.phase.set(self.t('Paused. Allow up to one minute for the radio link to return to normal, then restart with the same file.'))
                    else:
                        self.phase.set(self.t('Transfer failed. Open the transfer folder for details.'))
                    if self.run_directory:
                        for keyfile in ('target.key', 'route.key'):
                            (self.run_directory / keyfile).unlink(missing_ok=True)
                    if self.close_when_done:
                        self.destroy()
                        return
        except queue.Empty: pass
        self.poll_job = self.after(100, self.poll)

    def pause(self):
        if self.process and not self.confirming:
            self.paused = True
            from operator_config import private_write
            private_write(self.run_directory / 'pause.request', 'pause\n')
            self.pause_button.configure(state='disabled')
            self.phase.set(self.t('Pausing after the current radio exchange…'))

    def open_folder(self):
        if not self.run_directory: return
        if sys.platform == 'win32': os.startfile(self.run_directory)
        else: webbrowser.open(self.run_directory.as_uri())

    def destroy(self):
        job = getattr(self, 'poll_job', None)
        if job:
            try: self.after_cancel(job)
            except tk.TclError: pass
            self.poll_job = None
        super().destroy()

    def close(self):
        if self.process:
            if self.confirming:
                messagebox.showinfo(self.t('Transfer in progress'), self.t('Boot confirmation is in progress. Please wait for completion.'), parent=self)
                return
            if not messagebox.askyesno(self.t('Transfer in progress'), self.t('Pause this transfer and close?'), parent=self): return
            self.close_when_done = True
            self.pause()
            return
        self.destroy()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--settings-dir', type=Path)
    args = parser.parse_args()
    try: settings = Settings(args.settings_dir)
    except (OSError, ValueError, KeyError) as error:
        root = tk.Tk(); root.withdraw()
        messagebox.showerror('Invalid settings file', str(error), parent=root)
        root.destroy()
        return 1
    OperatorApp(settings).mainloop()
    return 0


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--ota-engine':
        arguments = sys.argv[2:]
        if arguments[:1] == ['--operator-output']:
            output = open(arguments[1], 'a', encoding='utf-8', buffering=1)
            sys.stdout = sys.stderr = output
            arguments = arguments[2:]
        sys.argv = [sys.argv[0], *arguments]
        import send_integrated
        try:
            code = send_integrated.main()
        except Exception:
            import traceback
            traceback.print_exc()
            code = 1
        raise SystemExit(code)
    else:
        raise SystemExit(main())
