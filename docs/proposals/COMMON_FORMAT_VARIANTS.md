# Common native and PPM format — measured variants

Working document, 27 October 2026. It prepares the choice of a common frame
format to be discussed with Walter K6ATV and Jon N7UV and then, for the
selected variant, with Bernard F6BVP (regulations) and F4HOF (on-air
framing), before any proposal to Ricardo's firmware.

## Goal

A single frame format that reduces airtime, that a relay can handle
(decompress, update the path, optionally add an RXT measurement, recompress)
and that complies with the rules on station identification. The native format
structures the frame and ties each RXT measurement to the relay that made it;
Walter's PPM compresses a byte string reversibly. The two are not in
opposition: the question is what one structures and what the other
compresses.

## Regulations

French regulation (ARCEP decision no. 2012-1241) forbids, in article 1,
"coding transmissions between amateur stations to obscure their meaning", and
requires, in article 4, amateur stations to "transmit their call sign" at
short intervals, at least at the beginning and end of each transmission
period (our translation). It requires neither ASCII nor readability without
decompression. A published, reversible coding, whose public decoder recovers
the call sign, does not obscure the meaning; the mere use of compression is
not an infringement.

A call sign written in clear is therefore not an elimination criterion. It is
a technical option, which lets a station without the decoder know who is
transmitting, and this document measures its cost as such.

Article 4 concerns the transmitting station and requires identification per
transmission period, not per frame. For a relayed frame, the transmitting
station is the relay. Our design rule, which goes beyond the regulatory
requirement, is that every frame transmitted by a relay carries its call
sign: in every variant the relay writes it into the path; the only question
is in what form. Bernard F6BVP, who suggested a reversible and published
form, will be asked for his opinion on the selected variant, based on this
text.

## Frame sizes

All measurements use the same captures as the 25 October studies: 26,303
receptions in Ariège (France) and 990 packets from Jon's network in Arizona.
Sizes include the 3-byte radio prefix, without any RXT block. Every
compressed frame was decoded and compared with the original. PC computation
(`logs/format-variants-20261027/`).

| Variant | Source in clear | Ariège, bytes saved | Ariège, SF12 airtime saved | Arizona, bytes saved | Arizona, SF7 airtime saved |
|---|---|---:|---:|---:|---:|
| Native, free text coded at best (Huffman or PPM) | no | 41.5 % | 31.9 % | 46.0 % | 40.4 % |
| Native, source as raw text | yes | 36.8 % | 28.3 % | 44.0 % | 38.6 % |
| PPM of the whole packet | no | 47.7 % | 36.5 % | 50.0 % | 44.0 % |
| Source in clear, then PPM of the rest | yes | 42.0 % | 32.3 % | 47.7 % | 41.9 % |
| Source in clear, then PPM of the whole packet | yes | 36.1 % | 27.7 % | 43.3 % | 38.1 % |

Savings are relative to the current text frames (72.5 bytes on average in
Ariège, 109.9 in Arizona).

Writing the source in clear costs 2 to 6 points of savings depending on the
corpus and the format. Among the variants that do so, the source in clear
followed by PPM of the rest of the packet is the most compact, roughly on par
with the current native format. The native format with the source in clear
comes next, 5 points lower in Ariège and 4 in Arizona. Repeating the source
in clear in front of a PPM of the whole packet is the weakest option.

These figures alone do not decide. The native format carries functions that
PPM lacks: structured fields and the attribution of each RXT measurement to
the relay that made it.

## RXT cost

In the native format, RXT measurements form a block of one mask byte, giving
the position in the path of each measured relay, followed by 4 bytes per
measurement. This block does not depend on how the rest of the frame is
coded: it can precede a PPM payload unchanged, the relay being identified by
its position in the decompressed path. The current text format costs 2 bytes
plus 5 per measurement. All compressed variants are therefore counted here
with the native block, and text with its current block.

In Ariège, the number of measurements comes from the SD log (1,023 packets
with one measurement and 14 with two, out of 26,303). The Arizona capture
does not record it: one measurement is assumed for every packet relayed at
least once (859 of 990). This is a scenario, not a guaranteed upper bound,
since a packet can carry up to three measurements.

These sizes with RXT are computed by adding the block cost to the previous
table; only frames without RXT were encoded and decoded.

| Variant | Source in clear | Ariège, bytes saved | Ariège, SF12 | Arizona, bytes saved | Arizona, SF7 |
|---|---|---:|---:|---:|---:|
| Native, free text at best | no | 41.4 % | 31.8 % | 45.1 % | 39.9 % |
| Native, source as raw text | yes | 36.7 % | 28.3 % | 43.2 % | 38.3 % |
| PPM of the whole packet | no | 47.6 % | 36.4 % | 48.9 % | 43.2 % |
| Source in clear, then PPM of the rest | yes | 42.0 % | 32.3 % | 46.7 % | 41.3 % |
| Source in clear, then PPM of the whole packet | yes | 36.0 % | 27.7 % | 42.6 % | 37.6 % |

RXT takes at most about one point off each variant and does not change the
ranking: with a shared block, it does not separate the variants. What
separates them is the processing done by a relay.

## Processing time on an ESP32

Measured on 27 October on a classic ESP32 at 240 MHz (TTGO LoRa32 V2.1,
F4MLV-15) with the same codecs, on 400 Ariège and 200 Arizona packets drawn
at random. A relay decodes the received frame and then encodes the frame it
sends; both operations are timed together for each packet, without changing
the path. Values in milliseconds. All 600 packets were decoded and
re-encoded identically.

| Coding | Ariège, mean | Ariège, 95th percentile | Ariège, maximum | Arizona, mean | Arizona, 95th percentile | Arizona, maximum |
|---|---:|---:|---:|---:|---:|---:|
| Native, Huffman text | 7.9 | 17.7 | 40.6 | 19.5 | 32.7 | 40.8 |
| Native, text at best (Huffman or PPM) | 19.1 | 43.8 | 116.7 | 51.9 | 74.9 | 117.6 |
| PPM of the whole packet | 39.6 | 59.4 | 89.1 | 61.2 | 79.5 | 94.7 |
| Source in clear, then PPM of the rest | 34.5 | 54.1 | 84.3 | 56.8 | 74.1 | 90.8 |

Packets that PPM declines to compress (2 of 400 in Ariège for the whole
packet, 1 for the rest) are sent as text and are not timed.

At SF12, where a compressed frame lasts about 2 s, PPM adds on average about
2 % to the relay delay, and about 5 % in the worst case observed; native at
best reaches about 6 % in its worst case. This computation does not occupy
the channel. At SF7, where a frame lasts about 130 ms, PPM adds on average a
delay of about half the frame duration (61 ms), up to 95 ms; this is still
relay latency, not channel occupancy, but native Huffman is three times
faster there on average.

Memory: the PPM decoder adds about 127 KiB of flash to the iGate firmware. In
the benchmark firmware, the whole stack used, codecs included, stayed below
3.3 KiB of the main task's 8 KiB. The codecs' working memory was not isolated
from that of the benchmark program.

## Remaining measurements

The codecs' working memory on the board, isolated from the rest, and the
complete processing in the real firmware (path update and RXT included)
rather than in a separate benchmark program. The 127 KiB of flash comes from
comparing the 27 October builds (1,763,585 bytes for GENATX's PPM build
against 1,633,921 for the `rxt` build).

The relay's call sign: if the call-sign-in-clear option is kept, what
matters is the transmitting station's call sign; a variant in which the
relay writes its own call sign in clear, not only the source, must then be
measured.

Robustness: rejection of truncated frames and unknown types, and versioning
of the PPM model, which is frozen and identified in the prefix.

## Out of scope

PPM primed mode stays excluded until its reference check is strengthened.
Collisions between neighbouring relays are handled separately
(`docs/tests/2026-10-27/RELAY_COLLISIONS_20261027_fr.md`).
