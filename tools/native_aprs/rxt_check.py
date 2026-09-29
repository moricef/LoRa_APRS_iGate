"""RXT inside the native frame: round-trip on F4MLV-10 v2 transmissions, size vs text trailers."""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import codec
from roundtrip import CORPUS, toa


def fingerprint(call):
    h = 0x811C9DC5
    for c in call.upper().replace('*', '').encode():
        h = ((h ^ c) * 0x01000193) & 0xFFFFFFFF
    return chr(h % 89 + 33)


TR = re.compile(r'\{([!-y]+)\}$')
ok = bad = unattributed = n_frames = 0
airtime = {'v1': 0.0, 'v2': 0.0, 'native': 0.0}

for name in ('F4MLV-10_aprs_rx.old.csv', 'F4MLV-10_aprs_rx.csv'):
    for line in open(CORPUS / name, encoding='latin1'):
        p = line.rstrip('\n').split(',', 8)
        if len(p) < 9 or p[1] != 'RXT_TX' or p[8].startswith(('F4MLV-9>', 'F4MLV-8>')):
            continue
        m = TR.search(p[8])
        if not m:
            continue
        L = len(m.group(1))
        n = L // 4 if L in (4, 8, 12) else L // 5 if L in (5, 10, 15) else 0
        if not n:
            continue
        clean = p[8][:m.start()]
        if ':' not in clean or '>' not in clean.split(':', 1)[0]:
            continue
        head = clean.split(':', 1)[0]
        path = head.split('>', 1)[1].split(',')[1:]
        if n > len(path):
            continue
        n_frames += 1
        base = 3 + len(clean.encode('latin1'))
        airtime['v1'] += toa(base + 2 + 4 * n, 12, 1, 1)
        airtime['v2'] += toa(base + 2 + 5 * n, 12, 1, 1)
        # Native size depends only on the tuple count, not on which elements carry them.
        airtime['native'] += toa(len(codec.encode(clean, {k: '!!!!' for k in range(n)})), 12, 1, 1)

        if L % 5 == 0:
            # v2 transmission: attribute each tuple to its relay through the fingerprint.
            tuples = [m.group(1)[i:i + 5] for i in range(0, L, 5)]
            used = [k for k, e in enumerate(path) if not codec.WIDE_RE.match(e)]
            rxt, start = {}, 0
            for t in tuples:
                k = next((u for u in used[start:] if fingerprint(path[u]) == t[0]), None)
                if k is None:
                    unattributed += 1
                    break
                rxt[k] = t[1:]
                start = used.index(k) + 1
            else:
                b = codec.encode(clean, rxt)
                if codec.decode_full(b) == (clean, rxt):
                    ok += 1
                else:
                    bad += 1

print(f'v2 transmissions: exact frame+RXT round-trip {ok}, failures {bad}, unattributed {unattributed}')
print(f'{n_frames} frames transmitted with RXT, SF12 CR4/5 airtime:')
for k in ('v1', 'v2', 'native'):
    print(f'   {k:7} {airtime[k] / 1000:8.0f} s  ({100 * (airtime[k] - airtime["v1"]) / airtime["v1"]:+.1f} % vs v1 text)')
