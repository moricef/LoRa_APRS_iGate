# RF validation of the fixes — 28 September 2026

## Test setup and evidence

Tests were conducted between 20:35 and 20:42 UTC using F4MLV-15 and F4MLV-2,
with the T-Deck as an independent receiver. Both TTGO devices run the
`ttgo-lora32-v21_SD` variant, build `2026-09-28 19:34:05 UTC`, containing the
five fixes and the Mic-E fix included in `a85b3d6`.

Firmware SHA-256:
`eeb8f1a698f7201c0f23e3edb2fc9dfcb8c3f21c2c396f7e47e01640d9be45d6`.

Test frames are injected through F4MLV-2's TNC2 input and actually transmitted
over LoRa. F4MLV-15 is the device under test. Replies and digipeated frames
are received over RF by F4MLV-2 and/or the T-Deck and correlated with
F4MLV-15's serial capture. F4MLV-9 is a synthetic test identity, not a fourth
device. The already-used paths in the injected frames are synthetic.

Captures and scripts are retained locally in
`logs/validation-20260928/`. The `analyse.py` script checks **32 assertions**
against these captures; all initially passed, but the N1 check was insufficient:
it accepted an incorrect ACK source. This result therefore does not validate
the firmware for deployment. This directory also contains configuration
backups with secrets and must not be published as a whole.

## Results

| Case | Supporting observation | Conclusion limited to this test |
| --- | --- | --- |
| Repeated query `R1` | Two receptions approximately 12 s apart, two ACKs, only one query response; both iGate and digi enabled on F4MLV-15 | One ACK per reception, without a duplicate query response within the 25 s window |
| Reply-ack `R2` | Reception of `{R2}A1`, then `{R2}A2`; RF transmission and reception of `ackR2}A1` and `ackR2}A2`, with only one query response | Suffix echoed correctly; changing `AA` does not execute the query again |
| Local reply paths `P1/P2/P3` | ACKs and responses received with `F4MLV-2,RFONLY`, then `F4MLV-2,F4MLV-7,RFONLY`, then just `RFONLY`; `beacon.path=WIDE1-1` | Learned path used in reverse order; no relay added for a direct reception; `RFONLY` follows the relays |
| Learning without APRS-IS `N1` | Query addressed to tactical identity `F4MLV-1`; ACK sent by `F4MLV-15`, response sent by `F4MLV-1`; both use the learned path `F4MLV-2,RFONLY` | Correct path without APRS-IS, but **ACK identity fails**: its source must be `F4MLV-1` |
| Callsign case | Packet carrying `f4mlv-15` received and then digipeated with `f4mlv-15*` | Explicit callsign recognized in lowercase |
| Cross-frequency digipeating | Temporary tactical identity `F4MLV-1`; `F4MLV-10` in the path and then as the source does not prevent digipeating; `f4mlv-1*` is received but not digipeated | No substring-based identity confusion; loop protection preserved |
| Mic-E | Destinations `490350` and `490351`, with the same source and information field: two digipeated frames received; another copy of `490350` approximately 18 s later via a different path: DUP decision | Destinations distinguished; a path change alone still produces a duplicate |
| Mic-E return path | Successive learning via `F4MLV-2`, then `F4MLV-7` for the two destinations; the copy of the first via `F4MLV-8` does not replace the path | A new position can teach a route; its duplicate does not overwrite it |

For the cross-frequency test, F4MLV-15 receives on 433.775 MHz / SF12 / CR5
and transmits on 434.855 MHz / SF9 / CR7. F4MLV-2 uses the opposite profiles.
The digipeated frames captured by F4MLV-2 therefore actually passed through
both radio profiles.

## Payload size limit

The injected frames use the explicit path `F4MLV-15,RFONLY`.
Consuming that hop adds one `*` byte. The sizes below refer to the APRS text
frame, excluding the three-byte LoRa prefix.

| Received size | Size after marking the relay hop | Observed result |
| --- | --- | --- |
| 245 | 246 | Six RXT bytes added; complete 252-byte frame received by the T-Deck |
| 246 | 247 | Digipeated without new RXT; all 247 bytes received |
| 251 | 252 | Digipeated without new RXT; all 252 bytes received |
| 252 | 253 | Explicit rejection diagnostic before transmission; no corresponding transmission in the serial capture |

## Validation limits

- The local reply path tests validate the paths of ACKs and responses actually
  transmitted and received over RF. They do not demonstrate isolated delivery
  through two physical relays: the incoming paths are synthetic, and the
  T-Deck can hear F4MLV-15 directly.
- The T-Deck serves as a monitoring receiver here, not as a source of
  application-level ACKs for messages addressed to the synthetic identity
  F4MLV-9.
- Fallback to `beacon.path` when no route is known and wake-up from ecoMode
  are not exercised on hardware in this series.
- F4MLV-15 detects no SD card. The absence of an `RXT_TX` entry in an SD log
  when falling back to transmission without RXT is therefore not verified
  on hardware.
- Mic-E is exercised with synthetic frames to test duplicate suppression,
  not to validate geographic decoding of a Mic-E tracker.
- The ambiguity between an RXT suffix and a legitimate comment remains
  outside these fixes and this validation, pending discussion with Jon.

## Complementary series — 28 September 2026, 22:12–23:51 UTC

### Test setup

F4MLV-15 and F4MLV-2 run the `ttgo-lora32-v21_SD` variant, build
`2026-09-28 21:49:19 UTC`, commit `36ac992`, which fixes the ACK identity
issue revealed by N1. SHA-256:
`32e7c112bde9537ba1d30c7e516d1c8e52d71feb8b815a948b0466245a0db18c`.
The host tests pass (digi-host 64/64, including two responder-identity cases).

F4MLV-10, an in-service digipeater on the same frequency, has transmission
disabled during the series. F4MLV-2 injects through TNC2 and receives over RF;
the T-Deck serves as a monitoring receiver. F4MLV-5, F4MLV-6 and F4MLV-9 are
synthetic identities. Captures: `logs/validation-complementaire-20260928/`.

### Results

| Case | Supporting observation | Conclusion limited to this test |
| --- | --- | --- |
| Identity `I4` | Tactical `F4MLV-1` configured; query to `F4MLV-1`; ACK and response sent by `F4MLV-1` | The N1 identity failure is fixed |
| Identity `I3` | Same configuration; query to `F4MLV-15`; no response | The digipeater answers only the tactical callsign |
| Fallback `F3` | Source `F4MLV-5` never heard before, invalid incoming path; log `Invalid RF path for F4MLV-5`; ACK and response on `WIDE1-1,RFONLY` | Fallback to `beacon.path` without a known route |
| Third-party message `T1` | `}F4MLV-9>APRS,TCPIP,F4MLV-2*::F4MLV-15 :…{T1`; ACK and response on `WIDE1-1`, without `RFONLY` | Gateway behaviour preserved |
| `WIDE1-1` | Digipeated as `F4MLV-15*,RFONLY` | No regression |
| `WIDE2-2` | Digipeated as `F4MLV-15*,WIDE2-1,RFONLY` | No regression |
| `WIDE1-1,WIDE2-1` | Digipeated as `F4MLV-15*,WIDE2-1,RFONLY` | No regression |
| ecoMode `E3` | Query, then the same query 22 s later: two ACKs, one response | One ACK per reception and a single response after wake-up |
| ecoMode `E4` | `{E4}A1` then `{E4}A2`: `ackE4}A1`, `ackE4}A2`, one response | Correct reply-ack after wake-up |
| `!RC1` | `EM=OFF` command signed by F4MLV-2: `accepted at counter 20`, response `DigiEcoMode:OFF`, restart out of ecoMode; digipeated copy with the same counter: `duplicate ignored at counter 20` and another ACK `ack078` | Effect verified; the copy is not executed again |
| APRS-IS upload | Direct copy, then `F4MLV-2*` copy of `UPLOAD-TEST-2300`: two `Uploaded to APRS-IS` in F4MLV-15's log; the server forwards one copy, `qAR,F4MLV-15` | Each reception is uploaded; duplicate suppression stays on the server |

### Interoperability with the T-Deck — 23:15 UTC

F4MLV-15 back in its initial configuration (iGate, digi 0, APRS-IS enabled).
The T-Deck (Tracker firmware) sends `?APRSV{291` to `F4MLV-15` via
`WIDE1-1,WIDE2-1`.

| Time | Observation |
| --- | --- |
| 23:15:04 | F4MLV-15 receives the direct copy and learns `F4MLV-7 via DIRECT` |
| 23:15:06 | ACK `F4MLV-15>APLRG1,RFONLY::F4MLV-7  :ack291`; the T-Deck fails to receive it (`code -7`) |
| 23:15:11 | Response sent; another `code -7` failure on the T-Deck |
| 23:15:17 | F4MLV-15 receives the copy digipeated via `F4MLV-10,F6DEV-10*` |
| 23:15:20 | New ACK `ack291`, without a second execution; the T-Deck receives it and marks the message acknowledged |

Conclusion limited to this test: a real client accepts the ACK sent by the
addressed identity over the learned direct route, and a lost ACK is recovered
through the next copy without a second execution. The T-Deck did not receive
the query response. The cause of the `-7` errors
(`RADIOLIB_ERR_CRC_MISMATCH`) is not determined.

During this test, F4MLV-2, back in its iGate configuration, published the
message to APRS-IS; F4MLV-15 also processed it through that path
(`Rx Query (APRS-IS)`), without duplicate suppression against the RF
reception. This behaviour exists upstream and is outside the scope of the
fixes.

### Interoperability with Graywolf — 23:20 UTC

Graywolf, using F4MLV-2 as its radio, sends `?APRSV{079` to `F4MLV-15` via
`WIDE1-1,WIDE2-1`. F4MLV-15 receives the direct copy (route `DIRECT`), sends
`ack079` and then the response on `RFONLY`. It then receives a copy via
`F4MLV-10,F6DEV-10*` and sends a second `ack079`, without a second response.
Graywolf shows the outgoing message as `acked` (one retry) and the received
response.

Conclusion limited to this test: a second real client accepts the ACK and
receives the response. Graywolf used a plain message number `{079}`; reply-ack
with a real client is not exercised. The "Raw TNC-2" lines shown by Graywolf
are reconstructed and differ from the actual frames (destination, path,
addressee); this discrepancy belongs to Graywolf.

### Isolated physical delivery through two relays — 23:51 UTC

Profiles: A = 433.775 MHz SF12 CR5; B = 434.855 MHz SF9 CR7;
C = 434.300 MHz SF9 CR7, all at 125 kHz.

| Device | Receive | Transmit | Role |
| --- | --- | --- | --- |
| F4MLV-15 | B | A | iGate |
| F4MLV-2 | A | C | relay 1, digi mode 2 |
| F4MLV-10 | C | B | relay 2, digi mode 2 |
| F4MLV-7 (T-Deck) | B | B | terminal |

The T-Deck can hear neither F4MLV-15 (A) nor F4MLV-2 (C). F4MLV-10 injects
through its TNC the synthetic learning packet
`F4MLV-7>APLRT1,F4MLV-10,F4MLV-2*,RFONLY:>ROUTE-TEST-2`; `F4MLV-14` then
sends the message `Test deux relais{D202` to F4MLV-7 through APRS-IS.

| Time | Observation |
| --- | --- |
| 23:51:00 | F4MLV-15: `Learned F4MLV-7 via F4MLV-2,F4MLV-10` |
| 23:51:11 | F4MLV-15 transmits on A `F4MLV-15>APLRG1,F4MLV-2,F4MLV-10:}F4MLV-14>APRS,TCPIP,F4MLV-15*::F4MLV-7  :Test deux relais{D202` |
| 23:51:13 | The T-Deck receives `…,F4MLV-2,F4MLV-10*:}…{D202`, the only copy received |
| 23:51:20 | The T-Deck sends `ackD202` |
| 23:51:21 | F4MLV-15 receives the ACK on B and publishes it: `F4MLV-7>APLRT1,WIDE1-1,WIDE2-1,qAR,F4MLV-15::F4MLV-14 :ackD202` |

Conclusion limited to this test: the learned two-relay return path is
executed by both physical relays in order, the message is delivered to the
physical terminal by the second relay only, and the terminal's ACK returns to
APRS-IS. The learning path is synthetic; the ACK returns directly, without a
relay.

A first attempt (`D201`, 23:42 UTC) is not retained: the T-Deck was set to B
while the setup required C, and it received relay 1's copy.

### Finding established during the series

A tactical callsign disables APRS-IS at startup (`src/gps_utils.cpp:60-65`).
A message addressed to `Config.callsign` while a tactical callsign is
configured therefore cannot be handled by the iGate: this planned case does
not apply. The WebUI shows the value stored in the configuration file, not
this value forced in memory.

### Limits of the complementary series

- Interoperability verified with the T-Deck and Graywolf, both using a plain
  message number; reply-ack is verified with injected frames only.
- F4MLV-15 has no SD card.
- The 3 published variants compile (`ttgo-lora32-v21_SD`,
  `heltec_wifi_lora_32_V3_2`, `QRPLabs_LightGateway_Plus_1_0`); the other
  environments are not compiled.
- With the `WIDE1-1` fallback alias, the F3 ACK was also digipeated by the
  public digipeater F5ZQC-10.
