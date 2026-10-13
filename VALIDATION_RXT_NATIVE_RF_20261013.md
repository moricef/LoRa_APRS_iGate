# Preserving native RXT identity — RF tests, 13 October 2026

The fix preserves the association between a native measurement and its path
element during reception, relaying and queueing. These tests check that
association in the actual retransmitted frame when two path callsigns share
the same fingerprint.

## Setup and method

F4MLV-2 and F4MLV-15 are two TTGO LoRa32 v2.1 boards at the same location.
Test profile: 433.900 MHz, SF7, BW 125 kHz, CR 4/5, power 2 dBm, CAD disabled.
F4MLV-15 relays an explicit path and transmits native frames. F4MLV-10 is
not part of the setup.

F4MLV-2 transmits native frames constructed with the codec. Their path
contains F4MLV-18 and F6DEV-10, both with fingerprint `C`, with only F6DEV-10
carrying a measurement. These callsigns are constructed path entries, not
devices physically participating in the test. The values `bbbb` and `cccc`
are injected to check their preservation.

F4MLV-15 receives and relays. Displayed identity is checked in its JSON
events. Its retransmitted bytes are captured by F4MLV-2's radio and decoded
on the host; a correct display alone is therefore insufficient to validate
the relay.

For the first four cases, the receiver runs the corrected binary without
test instrumentation: build `2026-10-13 15:56:12 UTC`, SHA-256
`66bff0b00daca439e756b307a876bdb4bdd3531f2ffd850a351ae3b4ebd7e2d5`.
The transmitter runs an instrumented copy for injection and radio byte
capture. For the deferred case, the receiver runs that same copy, which
holds only packets marked `NRF-DEFER` for seven seconds without blocking
reception. Attribution and encoding use the corrected implementation;
the instrumentation does not change measurements or their owners.

## Confirmed observations

Path indexes below are zero-based: index 0 is the first element after the
destination.

| Case | Observation |
| --- | --- |
| `NRF-ALIAS-161713` | The measurement received at index 2 is attributed to F6DEV-10. After `WIDE1*` is removed, it is retransmitted at index 1, still as `bbbb`. F4MLV-15's local measurement is at index 2. |
| `NRF-PLAIN-161824` | With no alias to remove, `bbbb` remains at index 1 on F6DEV-10, with the new local measurement at index 2. |
| `NRF-COMMENT-161713` | The comment ending in `{Cbbbb}`, which resembles a v2 text trailer, remains unchanged in the native relay. The separate measurement remains attached to F6DEV-10. |
| `NRF-IS-161927` | The copy observed on APRS-IS preserves exactly `>NRF-IS-161927 {Cbbbb}`. No RXT is added to the information field. The native RF relay also preserves `bbbb` and its owner. |
| `NRF-DEFER-162139` followed by `NRF-AFTER-162139` | The second packet is received before the first is retransmitted. Their retransmissions respectively contain `bbbb` and `cccc` on F6DEV-10; the new reception has not replaced the queued packet's measurements. |

Capture timestamps for the deferred case:

- first JSON reception: `16:21:55.648397 UTC`;
- second JSON reception: `16:21:57.657004 UTC`;
- first retransmission captured over RF: `16:22:02.684851 UTC`;
- second retransmission captured over RF: `16:22:02.884774 UTC`.

## Evidence and scope

Results and captures are retained locally in `logs/native-rf-20261013/`:
`basic-results.json`, `plain-results.json`, `boundary-results.json`,
`deferred-results.json` and the `*-capture.jsonl` files. The host decoder,
test driver and instrumented sources are in the same directory, which is
ignored by git. These captures are not included with this report.

The observations validate preservation of received identity, index remapping
after alias removal, opaque comments, the APRS-IS boundary and queued
reception context. They concern two nearby boards on the stated profile;
they are not a range test or validation of every path and radio profile.
The native wire format is unchanged. A relay that subsequently chooses text
transmission returns to the fingerprint attribution limits of text RXT.
