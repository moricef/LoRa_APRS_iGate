# RXT v2 proposal — self-identifying tuples

## Why

An RXT v1 tuple doesn't say which digi produced it. To attribute it, the
decoder matches tuples against a whitelist of RXT callsigns found in the path.
When the whitelist is incomplete or differs between iGates, hops show up as
N/A or get attributed to the wrong digi.

There is also the trailer ambiguity we discussed: a beacon comment ending in
something like `{abcd}` looks exactly like RXT data. The digi appends its
tuple into it, and the iGate strips it before APRS-IS, so the user's text is
lost. The risk is low, but the loss is silent and can hit a station of any
brand.

## Format

Add one character in front of each tuple, identifying the digi that produced
it:

```
v1: RSSI SNR FO TTH        4 characters
v2: ID RSSI SNR FO TTH     5 characters
```

RSSI, SNR, FO (frequency offset) and TTH (time to hop) are encoded exactly as
in v1. The callsign itself is already in the path; `ID` is only there to find
it.

I first thought of using the digi's position in the path, but that breaks as
soon as a digi rewrites the path (consumed `WIDE1*`/`WIDE2*` removed, an alias
replaced by `CALL*,WIDE2-1`), or when a digi decrements an alias without
inserting its callsign. So `ID` is a fingerprint of the callsign instead.

## Fingerprint

Take the callsign as it appears in the path: upper case, SSID included, the
tactical callsign if the digi uses one, without the `*`. Run 32-bit FNV-1a
over its bytes (start `0x811C9DC5`, for each byte XOR then multiply by
`0x01000193`, mod 2^32), take the result mod 89 and add 33. That gives a
character between `!` and `y`, same range as the other fields.

Test vectors:

| Callsign | FNV-1a | mod 89 | ID |
| --- | --- | --- | --- |
| F4MLV-MC | `0xDBB682C7` | 40 | `I` |
| F4MLV-18 | `0xDDC21C0E` | 34 | `C` |
| F6DEV-10 | `0x6905079A` | 34 | `C` |
| F4MLV-2 | `0xD182B2AF` | 9 | `*` |
| F1ZDB-10 | `0x634631E2` | 9 | `*` |
| F4MLV-10 | `0xD5C20F76` | 71 | `h` |
| F5ZQC-10 | `0x237100C0` | 32 | `A` |

89 values obviously don't separate every callsign in the world, but they don't
have to: the fingerprint only picks among the two or three digis of one path.
On 133 callsigns from my logs, 1.28 % of pairs collide, close to the expected
1/89. When two digis in the same path share an ID, tuples are matched in path
order, since they are appended in relay order. A tuple whose ID matches
nothing in the path (a digi that didn't insert its callsign) is shown as an
unidentified relay rather than guessed. Attribution can only go wrong if two
digis with the same ID are in the path and one of them didn't add a tuple; the
measurement is then misattributed, not lost.

These rules have to be written down precisely, since any other software
reading v2 must reproduce them exactly.

## Trailer check

The same fingerprint lets the firmware tell RXT data from a comment. A trailer
is treated as RXT only if every tuple points to a digi actually used in the
path. The check is used both when relaying and when gating:

- relaying: if the existing `{...}` fails the check, the digi leaves it alone
  and opens a new `{...}` after it;
- gating: the final `{...}` is stripped before APRS-IS only if it passes.

A comment ending in `{abcde}` would have to match a used digi by chance, so
the risk becomes very small on both sides.

## v1 and v2 together

With at most 3 tuples, lengths never overlap: 4, 8 or 12 characters for v1, 5,
10 or 15 for v2. Three is plenty with the New-N paradigm (`WIDE1-1,WIDE2-1` at
most, `WIDE2-2` for a fixed station, and the fork limits regional aliases to 2
hops by default).

Only our boards run RXT today, so switching means reflashing them all with v2
and dropping the whitelist. A v2 iGate can still recognize a leftover v1
trailer by its length and ignore it.

Graywolf only sees hops already decoded by the iGate, so the format change
doesn't affect it, apart from possibly displaying an unidentified relay.

## Airtime

One extra byte per tuple. On the EU profile (433.775 MHz, SF12, 125 kHz, CR
4/5), over packet sizes from 40 to 220 bytes. Other profiles give different
absolute values but similar percentages:

| Tuples | Mean | Max | Packets affected |
| --- | --- | --- | --- |
| 1 | +33 ms (0.7 %) | +164 ms | 19 % |
| 2 | +65 ms (1.4 %) | +164 ms | 39 % |
| 3 | +98 ms (2.0 %) | +164 ms | 59 % |

LoRa frames grow in blocks (164 ms at SF12), so most packets don't get longer
at all and the rest gain one block. A 90-byte beacon is about 4 s on air.
