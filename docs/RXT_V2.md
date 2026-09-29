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

Nobody assigns the ID. Each digi computes its own from its callsign, so there
is no registry, no coordination between digis and nothing to run out of. The
ID does not have to be unique in an area: the full callsign is already in the
path, and the ID only picks among the two or three digis of one path.

On 133 callsigns from my logs, 1.28 % of pairs share an ID, close to the
expected 1/89. When two digis in the same path share an ID and both added a
tuple, the tuples are matched in path order, since they are appended in relay
order. If one of them did not add a tuple, the remaining one may be attributed
to the wrong digi: the measurement is then misattributed, not lost. A tuple
whose ID matches nothing in the path (a digi that didn't insert its callsign)
is shown as an unidentified relay rather than guessed; the fingerprint cannot
recover a callsign that is not in the path.

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
the risk becomes very small on both sides. It is not zero, but in v1 a comment
ending like `{abcd}` is always taken for RXT.

## Why not path position, a list or a capability beacon

Knowing which digis are RXT-capable is not enough to attribute tuples, even
if every digi on the network were RXT-capable:

- a digi can relay a packet without adding a tuple, when the tuple would push
  the frame over the LoRa payload limit (the firmware then relays the packet
  unchanged);
- paths are rewritten (consumed `WIDE1*`/`WIDE2*` removed, aliases decremented
  without inserting a callsign), so the n-th tuple is not reliably the n-th
  digi.

In both cases the tuples after the gap shift onto the wrong digis, and nothing
in the packet shows it. The same applies at the APRS-IS boundary: without an
ID, the iGate has to guess from a list whether the final `{...}` is RXT or the
user's text, and two iGates with different lists upload different packets.

A capability beacon says what a digi can do, not what it did to this packet.
A local beacon without path is heard only by stations that hear that digi
directly, and nothing guarantees that the iGate decoding the tuples is one of
them. It is still useful for discovery and can coexist with v2; the decoding
just doesn't depend on it.

The ID is not a receiver measurement. It is there so that each packet carries
what is needed to read it, with no state kept on the network.

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

One extra byte per tuple. Counted against the RXT field alone that is one byte
in five; what matters on the channel is the airtime of the whole frame.

On real frames: F4MLV-10's SD log holds 12,490 frames it transmitted with an
RXT trailer (12,320 in v1, 170 in v2; 11,977 with one tuple, 511 with two, 2
with three). Keeping each frame's size and tuple count and computing its
airtime with 4 and then 5 characters per tuple:

| Profile | Mean v1 frame | v2 overhead | Frames lengthened | Block |
| --- | --- | --- | --- | --- |
| EU: SF12, 125 kHz, CR 4/5 | 3,383 ms | +0.72 % | 15 % | 164 ms |
| SF7, 125 kHz, CR 4/6 | 168 ms | +1.35 % | 37 % | 6.1 ms |

This is a calculation on transmitted v1 frames, not a measurement of v2
traffic. The frames come from our network; frames with longer comments give
a lower percentage.

Same calculation on your network, from the public TNC feed
`n7uv1.duckdns.org:33001` on 29 September 2026, 15:14–16:54 UTC: 957 frames
from 39 stations, 384 of them with tuples (322 with one, 61 with two, 1 with
three). The feed shows frames without their trailer and lists the decoded
hops separately, so the tuple count comes from the hop lines that are not
`NA`, and the v1 trailer is rebuilt as 4 characters per tuple plus braces.

| Profile | Mean v1 frame | v2 overhead | Frames lengthened | Block |
| --- | --- | --- | --- | --- |
| SF7, 125 kHz, CR 4/6 | 238 ms | +0.87 % | 34 % | 6.1 ms |

Over synthetic packet sizes from 40 to 220 bytes, EU profile:

| Tuples | Mean | Max | Packets affected |
| --- | --- | --- | --- |
| 1 | +33 ms (0.7 %) | +164 ms | 19 % |
| 2 | +65 ms (1.4 %) | +164 ms | 39 % |
| 3 | +98 ms (2.0 %) | +164 ms | 59 % |

LoRa frames grow in blocks (164 ms at SF12), so most packets don't get longer
at all and the rest gain one block. A 90-byte beacon is about 4 s on air.
