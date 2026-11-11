#!/usr/bin/env python3
"""Developer packaging helper. Build on Windows for a Windows executable."""
from pathlib import Path
import argparse
import subprocess
import sys

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]

def command(output):
    return [sys.executable,'-m','PyInstaller','--noconfirm','--clean','--onefile','--windowed',
            '--name','LoRaFirmwareUpdate','--paths',str(HERE),
            '--hidden-import','send_integrated','--hidden-import','window_transfer',
            '--distpath',str(output),'--workpath',str(ROOT/'.pio/operator-package/work'),
            '--specpath',str(ROOT/'.pio/operator-package'),str(HERE/'operator_ui.py')]

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'.pio/operator-package/dist')
    parser.add_argument('--show-command',action='store_true')
    args=parser.parse_args()
    if args.show_command:
        import shlex
        print(shlex.join(command(args.output)))
    else:
        (ROOT/'.pio/operator-package').mkdir(parents=True,exist_ok=True)
        subprocess.run(command(args.output),check=True)
