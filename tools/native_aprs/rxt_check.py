"""RXT inside the native frame: round-trip on F4MLV-10 v2 transmissions, size vs text trailers."""
import collections
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import native_tool
from roundtrip import CORPUS, toa

TR = re.compile(r'\{([!-y]+)\}$')
WIDE = re.compile(r'^WIDE[1-7](-[1-7])?\*?$')


def fingerprint(call):
    h = 0x811C9DC5
    for c in call.upper().replace('*', '').encode():
        h = ((h ^ c) * 0x01000193) & 0xFFFFFFFF
    return chr(h % 89 + 33)


def transmissions():
    """(frame without trailer, path, trailer content) for each RXT_TX line."""
    for name in ('F4MLV-10_aprs_rx.old.csv', 'F4MLV-10_aprs_rx.csv'):
        for line in open(CORPUS / name, encoding='latin1'):
            p = line.rstrip('\n').split(',', 8)
            if len(p) < 9 or p[1] != 'RXT_TX' or p[8].startswith(('F4MLV-9>', 'F4MLV-8>')):
                continue
            m = TR.search(p[8])
            if not m or len(m.group(1)) not in (4, 8, 12, 5, 10, 15):
                continue
            clean = p[8][:m.start()]
            if ':' not in clean or '>' not in clean.split(':', 1)[0]:
                continue
            path = clean.split(':', 1)[0].split('>', 1)[1].split(',')[1:]
            yield clean, path, m.group(1)


def attribute(path, content):
    """v2 content -> {path index: 4-char tuple}, or None if a tuple matches no used element."""
    used = [k for k, e in enumerate(path) if not WIDE.match(e)]
    rxt, start = {}, 0
    for i in range(0, len(content), 5):
        t = content[i:i + 5]
        k = next((u for u in used[start:] if fingerprint(path[u]) == t[0]), None)
        if k is None:
            return None
        rxt[k] = t[1:]
        start = used.index(k) + 1
    return rxt


def main():
    rows = [r for r in transmissions() if len(r[2]) // (5 if len(r[2]) % 5 == 0 else 4) <= len(r[1])]
    counts = [len(c) // (5 if len(c) % 5 == 0 else 4) for _, _, c in rows]
    v2 = [(clean, attribute(path, c)) for clean, path, c in rows if len(c) % 5 == 0]
    status = collections.Counter(native_tool.check([(t, r) for t, r in v2 if r is not None]))
    print(f'v2 transmissions: {len(v2)}, unattributed {sum(1 for _, r in v2 if r is None)}, round-trip {dict(status)}')

    sized = native_tool.encode([(clean, {k: '!!!!' for k in range(n)}) for (clean, _, _), n in zip(rows, counts)])
    air = {'v1': 0.0, 'v2': 0.0, 'native': 0.0}
    for (clean, _, _), n, b in zip(rows, counts, sized):
        base = 3 + len(clean.encode('latin1'))
        air['v1'] += toa(base + 2 + 4 * n, 12, 1, 1)
        air['v2'] += toa(base + 2 + 5 * n, 12, 1, 1)
        air['native'] += toa(len(b), 12, 1, 1)
    print(f'{len(rows)} frames transmitted with RXT, SF12 CR4/5 airtime:')
    for k in ('v1', 'v2', 'native'):
        print(f'   {k:7} {air[k] / 1000:8.0f} s  ({100 * (air[k] - air["v1"]) / air["v1"]:+.1f} % vs v1 text)')


if __name__ == '__main__':
    main()
