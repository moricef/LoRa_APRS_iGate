"""Batch access to native_aprs_tool (the C++ codec) for the analysis scripts."""
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent

def _run(mode, cases):
    subprocess.run(['make', '-s', '-C', str(HERE)], check=True)
    lines = []
    for tnc2, rxt in cases:
        spec = ';'.join(f'{k}:{v.encode("latin1").hex()}' for k, v in sorted(rxt.items()))
        lines.append(spec + '\t' + tnc2.encode('latin1').hex())
    out = subprocess.run([str(HERE / 'native_aprs_tool'), mode], input='\n'.join(lines) + '\n',
                         capture_output=True, text=True, check=True).stdout.splitlines()
    assert len(out) == len(cases)
    return out

def encode(cases):
    """[(tnc2, {index: tuple})] -> [native bytes or None]."""
    return [None if o == 'ERR' else bytes.fromhex(o) for o in _run('encode', cases)]

def check(cases):
    """[(tnc2, rxt)] -> ['OK', 'MISMATCH' or 'ERR'] (exact round-trip through the C++ codec)."""
    return [o.split()[0] for o in _run('check', cases)]

def free_text(frames):
    """[tnc2] -> [bytes of free text the frame carries]."""
    return [bytes.fromhex(o) for o in _run('text', [(t, {}) for t in frames])]
