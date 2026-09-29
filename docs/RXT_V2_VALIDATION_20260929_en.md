# RXT v2 validation — 29 September 2026

## Setup

F4MLV-15, F4MLV-2 and F4MLV-10 run `feature/rxt-v2`, variant
`ttgo-lora32-v21_SD`: build `2026-09-29 12:04:17 UTC` (`95a559d`) for the
first two, build `2026-09-29 12:33:37 UTC` (`7ff658c`, with `/sd/log`) for
F4MLV-10, the only one with an SD card. F4MLV-2 injects through its TNC and
decodes; F4MLV-15 and F4MLV-10 relay in digi mode 2 with RXT enabled; the
T-Deck (F4MLV-7) is a non-RXT relay in repeater mode. F4MLV-9 is a synthetic
identity. Captures: `logs/rxtv2-20260929/`.

Host tests: all five suites pass, including the fingerprint test vectors, the
trailer check when relaying and before APRS-IS, attribution in path order
with identical fingerprints, and the documented chance collision.

## Chain through two RXT relays — 12:17 UTC

Injected frame: `F4MLV-9>APLRG1,WIDE1-1,F4MLV-15,F4MLV-10,RFONLY:>RXTV2-TEST`.

| Copy received by F4MLV-2 | Decoded hops |
| --- | --- |
| after the T-Deck | `F4MLV-7<--F4MLV-9 NA` |
| after F4MLV-15 | `F4MLV-15<--F4MLV-7 RSSI:-76 SNR:+10.50`, then the NA line |
| after F4MLV-10 | `F4MLV-10<--F4MLV-15 RSSI:-64 SNR:+11.50` and `F4MLV-15<--F4MLV-7 RSSI:-76 SNR:+10.50`, then the NA line |

Both tuples are attributed to the right relays without a whitelist, the
non-RXT relay shows as NA, and the F4MLV-15 measurement is unchanged after the
second relay.

The same frame ending in `{world}` gives the same hops, and the decoded frame
keeps `RXTV2-TEST2 {world}` intact.

Outside the test, a public beacon from `F4KOL-4` relayed by `F5ZQC-10`
(non-RXT) then by F4MLV-15 was decoded with `F5ZQC-10` as NA and the F4MLV-15
tuple correctly attributed.

## Gating to APRS-IS — 12:29 UTC

F4MLV-15 in digi mode 2 with APRS-IS off; F4MLV-2, which only receives
relayed copies and therefore tuples, is the only iGate of the setup to gate
them. Frames injected without `RFONLY`, path `F4MLV-15,F4MLV-10`.

| Received by APRS-IS | Finding |
| --- | --- |
| `…,F4MLV-15*,F4MLV-10,qAR,F4MLV-2:>RXTV2-IS-1229` | copy carrying a tuple gated without trailer |
| `…,qAR,F4MLV-2:>RXTV2-IS-1229B {world}` | tuple removed, `{world}` intact |
| `…,qAR,F4BPJ-10:>RXTV2-IS-1229{\jf$?hcn/?}` | third-party iGate: trailer passed through |

The F4MLV-2 serial log shows two uploads per frame (copies from F4MLV-15 then
F4MLV-10); the second, identical once stripped, was deduplicated by APRS-IS.

## F4MLV-10 SD log

Read remotely with `GET /sd/log?tail=65536` (65,536 bytes in 1.1 s).

| Event | Trailer |
| --- | --- |
| `PATH` on the direct copy | not its turn yet |
| `RELAY` on the F4MLV-15 copy | received tuple `\jf$?`, ID `\` of F4MLV-15 |
| `RXT_TX` | sent tuple `hcn/?`, ID `h` of F4MLV-10; frame `…RXTV2-IS-1229{\jf$?hcn/?}` |
| `RXT_TX` for `{world}` | frame `…RXTV2-IS-1229B {world}{\ju$@hgn/A}`, comment intact, separate trailer |

## Limits

- iGates outside the fork (here F4BPJ-10) pass RXT trailers to APRS-IS
  unchanged, in v1 as in v2.
- Jon N7UV's network is not tested; its digis still run v1, which is not
  compatible with v2.
- Shape-only removal, used for the duplicate key where no path is available,
  is only covered by host tests.
