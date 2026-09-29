"""Round-trip the native prototype over the local corpora and estimate airtime."""
import collections, math, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import codec

CORPUS = Path(__file__).resolve().parents[2] / 'logs' / 'corpus-20260929'

def toa(pl, SF, CR, DE, BW=125000):
    Ts = 2**SF / BW
    n = 8 + max(math.ceil((8*pl - 4*SF + 28 + 16) / (4*(SF - 2*DE))) * (CR + 4), 0)
    return (12.25 + n) * Ts * 1000

def f4mlv10():
    for name in ('F4MLV-10_aprs_rx.old.csv', 'F4MLV-10_aprs_rx.csv'):
        for line in open(CORPUS / name, encoding='latin1'):
            p = line.rstrip('\n').split(',', 8)
            if len(p) < 9 or p[1] not in ('RELAY', 'PATH', 'DUP', 'DROP'): continue
            t = p[8]
            if ':' in t and '>' in t.split(':', 1)[0] and not t.startswith(('F4MLV-9>', 'F4MLV-8>')):
                yield t

def n7uv():
    for l in open(CORPUS / 'N7UV_feed.log', encoding='latin1'):
        t = l.rstrip('\n')[21:].rstrip()
        if t and not t.startswith('LOCAL --') and '<--' not in t and ':' in t and '>' in t.split(':', 1)[0]:
            head = t.split(':', 1)[0]
            if ',qA' in head or 'TCPIP' in head:    # APRS-IS traffic, not RF
                continue
            yield t

def run(label, frames, SF, CR, DE):
    n = fail = 0; cur = nat = 0.0; kinds = collections.Counter(); raw_elems = collections.Counter()
    for t in frames:
        n += 1
        try:
            b = codec.encode(t)
        except ValueError as e:
            fail += 1; kinds['error ' + str(e)] += 1; continue
        if codec.decode(b) != t:
            fail += 1; print('MISMATCH', repr(t)[:120]); continue
        kinds['position' if b[3] & 0x0F == codec.POSITION else 'raw'] += 1
        cur += toa(3 + len(t.encode('latin1')), SF, CR, DE); nat += toa(len(b), SF, CR, DE)
        head = t.split(':', 1)[0]; src, rest = head.split('>', 1)
        for e in [src] + rest.split(','):
            if not codec.WIDE_RE.match(e) and not codec.CALL_RE.match(e): raw_elems[e.rstrip('*')] += 1
    print(f'{label}: {n} trames, aller-retour exact {n - fail}, échecs {fail}, {dict(kinds)}')
    print(f'   temps d\'antenne {cur/1000:.0f} s -> {nat/1000:.0f} s ({100*(nat-cur)/cur:+.1f} %)')
    print('   éléments en texte brut les plus fréquents:', raw_elems.most_common(6))

if __name__ == '__main__':
    run('F4MLV-10 (SF12 CR4/5)', f4mlv10(), 12, 1, 1)
    run('N7UV (SF7 CR4/6)', n7uv(), 7, 2, 0)
