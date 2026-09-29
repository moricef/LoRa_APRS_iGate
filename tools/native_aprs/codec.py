"""Prototype codec for a native LoRa APRS frame (draft 0, not a specification).

encode(tnc2) -> bytes and decode(bytes) -> tnc2 must round-trip byte for byte.
Anything the draft cannot represent exactly is carried as raw text.
Strings are latin-1, so every byte value maps to one character.
"""
import re

PREFIX = b'<\xff\x02'

# ---- address elements -----------------------------------------------------
B40 = ' ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'
CALL_RE = re.compile(r'^([A-Z0-9]{1,6})(?:-([1-9]\d?|1\d\d|2[0-4]\d|25[0-5]))?(\*?)$')
WIDE_RE = re.compile(r'^WIDE([1-7])(?:-([1-7]))?(\*?)$')

def enc_element(e):
    m = WIDE_RE.match(e)
    if m:                                   # 0b0 s nnn NNN (N=0: no -N suffix)
        return bytes([(0x40 if m.group(3) else 0) | int(m.group(1)) << 3 | int(m.group(2) or 0)])
    m = CALL_RE.match(e)
    if m:                                   # 0b10 s xxxxx + 4 bytes base 40 [+ SSID byte]
        v = 0
        for ch in m.group(1).ljust(6):
            v = v * 40 + B40.index(ch)
        ssid = int(m.group(2) or 0)
        star = 0x20 if m.group(3) else 0
        if ssid <= 30:
            return bytes([0x80 | star | ssid]) + v.to_bytes(4, 'big')
        return bytes([0x80 | star | 31]) + v.to_bytes(4, 'big') + bytes([ssid])
    raw = e.encode('latin1')                # 0b11 lllll + raw text
    if not 1 <= len(raw) <= 31:
        raise ValueError('address element length')
    return bytes([0xC0 | len(raw)]) + raw

def dec_element(b, i):
    t = b[i]
    if t < 0x80:
        n, N = t >> 3 & 7, t & 7
        return f'WIDE{n}' + (f'-{N}' if N else '') + ('*' if t & 0x40 else ''), i + 1
    if t < 0xC0:
        v = int.from_bytes(b[i + 1:i + 5], 'big'); s = ''
        for _ in range(6):
            v, r = divmod(v, 40); s = B40[r] + s
        ssid = t & 0x1F; j = i + 5
        if ssid == 31:
            ssid = b[j]; j += 1
        return s.rstrip() + (f'-{ssid}' if ssid else '') + ('*' if t & 0x20 else ''), j
    n = t & 0x1F
    return b[i + 1:i + 1 + n].decode('latin1'), i + 1 + n

# ---- position -------------------------------------------------------------
DTIS = '!=/@'
TS_RE = re.compile(r'^(\d{6})([zh/])')
UNC_RE = re.compile(r'^(\d\d)(\d\d)\.(\d\d)([NS])(.)(\d{3})(\d\d)\.(\d\d)([EW])(.)')
CMP_RE = re.compile(r'^([/\\A-Za-j])([!-{]{4})([!-{]{4})(.)(.)(.)(.)', re.S)
EXT_RE = re.compile(r'^(\d{3})/(\d{3})')
ALT_RE = re.compile(r'/A=(\d{6}|-\d{5})')

def b91(s):
    v = 0
    for ch in s:
        v = v * 91 + ord(ch) - 33
    return v

def to_b91(v):
    s = ''
    for _ in range(4):
        v, r = divmod(v, 91); s = chr(r + 33) + s
    return s

def enc_position(info):
    dti = info[0]; body = info[1:]; flags = DTIS.index(dti); out = b''
    if dti in '/@':
        m = TS_RE.match(body)
        if not m:
            return None
        flags |= 0x04
        out += (int(m.group(1)) << 2 | 'zh/'.index(m.group(2))).to_bytes(3, 'big')
        body = body[7:]
    m = CMP_RE.match(body)
    if m and not body[:1].isdigit():
        flags |= 0x08
        lat, lon = b91(m.group(2)), b91(m.group(3))
        out += (lat << 27 | lon).to_bytes(7, 'big') + m.group(1).encode('latin1') + m.group(4).encode('latin1')
        c, s, T = m.group(5), m.group(6), m.group(7)
        if c == ' ' and s == ' ':
            flags |= 0x40; out += T.encode('latin1')
        else:
            out += (c + s + T).encode('latin1')
        rest = body[13:]
    else:
        m = UNC_RE.match(body)
        if not m:
            return None
        lat = int(m.group(1)) * 6000 + int(m.group(2)) * 100 + int(m.group(3))
        lon = int(m.group(6)) * 6000 + int(m.group(7)) * 100 + int(m.group(8))
        if lat >= 1 << 20 or lon >= 1 << 21:
            return None
        v = (lat << 1 | (m.group(4) == 'S')) << 22 | (lon << 1 | (m.group(9) == 'W'))
        out += v.to_bytes(6, 'big') + m.group(5).encode('latin1') + m.group(10).encode('latin1')
        rest = body[19:]
        m = EXT_RE.match(rest)
        if m:
            flags |= 0x10
            out += (int(m.group(1)) << 10 | int(m.group(2))).to_bytes(3, 'big')
            rest = rest[7:]
    m = ALT_RE.search(rest)
    if m and m.start() < 256:
        flags |= 0x20
        out += bytes([m.start()]) + (int(m.group(1)) & 0xFFFFFF).to_bytes(3, 'big')
        rest = rest[:m.start()] + rest[m.end():]
    return bytes([flags]) + out + rest.encode('latin1')

def dec_position(b):
    flags = b[0]; i = 1; s = DTIS[flags & 3]
    if flags & 0x04:
        v = int.from_bytes(b[i:i + 3], 'big'); i += 3
        s += f'{v >> 2:06d}' + 'zh/'[v & 3]
    if flags & 0x08:
        v = int.from_bytes(b[i:i + 7], 'big'); i += 7
        sym_t, sym_c = chr(b[i]), chr(b[i + 1]); i += 2
        if flags & 0x40:
            cst = '  ' + chr(b[i]); i += 1
        else:
            cst = b[i:i + 3].decode('latin1'); i += 3
        s += sym_t + to_b91(v >> 27) + to_b91(v & ((1 << 27) - 1)) + sym_c + cst
    else:
        v = int.from_bytes(b[i:i + 6], 'big'); i += 6
        la, lo = v >> 22, v & ((1 << 22) - 1)
        lat, ns = la >> 1, 'S' if la & 1 else 'N'
        lon, ew = lo >> 1, 'W' if lo & 1 else 'E'
        sym_t, sym_c = chr(b[i]), chr(b[i + 1]); i += 2
        s += f'{lat // 6000:02d}{lat // 100 % 60:02d}.{lat % 100:02d}{ns}{sym_t}'
        s += f'{lon // 6000:03d}{lon // 100 % 60:02d}.{lon % 100:02d}{ew}{sym_c}'
        if flags & 0x10:
            v = int.from_bytes(b[i:i + 3], 'big'); i += 3
            s += f'{v >> 10:03d}/{v & 1023:03d}'
    alt = None
    if flags & 0x20:
        off = b[i]; v = int.from_bytes(b[i + 1:i + 4], 'big'); i += 4
        if v & 0x800000: v -= 1 << 24
        alt = (off, f'/A={v:06d}' if v >= 0 else f'/A=-{-v:05d}')
    rest = b[i:].decode('latin1')
    if alt:
        rest = rest[:alt[0]] + alt[1] + rest[alt[0]:]
    return s + rest

# ---- frame ----------------------------------------------------------------
RAW, POSITION = 0, 1
RXT_FLAG = 0x08                          # header bit: RXT block after the path

def enc_rxt(rxt, npath):
    """rxt: {path index: 4-char tuple (RSSI SNR FO TTH)} -> mask byte + 4 bytes per tuple."""
    mask = 0; out = b''
    for k in sorted(rxt):
        if not 0 <= k < min(npath, 8) or len(rxt[k]) != 4 or any(not 33 <= ord(c) <= 122 for c in rxt[k]):
            raise ValueError('RXT tuple')
        mask |= 1 << k
        out += bytes(ord(c) - 33 for c in rxt[k])
    return bytes([mask]) + out

def dec_rxt(b, i):
    mask = b[i]; i += 1; rxt = {}
    for k in range(8):
        if mask >> k & 1:
            rxt[k] = ''.join(chr(x + 33) for x in b[i:i + 4]); i += 4
    return rxt, i

def encode(tnc2, rxt=None):
    head, info = tnc2.split(':', 1)
    src, rest = head.split('>', 1)
    parts = rest.split(',')
    dest, path = parts[0], parts[1:]
    if len(path) > 15:
        raise ValueError('path too long')
    body, kind = None, RAW
    if info[:1] in DTIS:
        body = enc_position(info)
        if body is not None:
            kind = POSITION
    if body is None:
        body = info.encode('latin1')
    def build(kind, body):
        out = PREFIX + bytes([len(path) << 4 | (RXT_FLAG if rxt else 0) | kind])
        out += enc_element(src) + enc_element(dest)
        for e in path:
            out += enc_element(e)
        if rxt:
            out += enc_rxt(rxt, len(path))
        return out + body
    out = build(kind, body)
    if decode_full(out) != (tnc2, rxt or {}):   # never emit a frame that does not round-trip
        out = build(RAW, info.encode('latin1'))
    return out

def decode(b):
    return decode_full(b)[0]

def decode_full(b):
    if b[:3] != PREFIX:
        raise ValueError('not a native frame')
    npath, kind = b[3] >> 4, b[3] & 0x07
    src, i = dec_element(b, 4)
    dest, i = dec_element(b, i)
    path = []
    for _ in range(npath):
        e, i = dec_element(b, i); path.append(e)
    rxt = {}
    if b[3] & RXT_FLAG:
        rxt, i = dec_rxt(b, i)
    info = dec_position(b[i:]) if kind == POSITION else b[i:].decode('latin1')
    return src + '>' + ','.join([dest] + path) + ':' + info, rxt
