# T-Deck native trial and RXT trailers on APRS-IS — 30 September 2026

## Native frames in real service

The T-Deck F4MLV-7 stayed on a desk, beaconing every 5 minutes (Smart Beacon
off) with digipeater mode off. It sent native frames from 07:05:41 to
09:02:34 UTC, then text from 09:02:34 to 10:07:46 UTC. Every other station sent
text and could receive native frames.

Receptions were taken from the TNC streams of F4MLV-2 and F4MLV-15, recorded on
a PC, and from the SD log of F4MLV-10. That log only gives time since boot; its
times were rebuilt, and each reception was matched to the nearest beacon seen by
F4MLV-2 or F4MLV-15, since the F4MLV-10 clock drifted by 25 to 46 seconds over
the morning. F1ZDB-10, too far away, heard no beacon directly. The 8 beacons the
T-Deck also sent straight to APRS-IS before 09:03:44 are left out.

| Phase | Beacons seen | F4MLV-2 | F4MLV-15 | F4MLV-10 |
| --- | --- | --- | --- | --- |
| Native (117 min) | 22 | 22, −79 dBm, SNR 11.6 dB | 18, −64 dBm, SNR 11.6 dB | 16, −88 dBm, SNR 11.4 dB |
| Text (65 min) | 11 | 11, −80 dBm, SNR 11.4 dB | 10, −64 dBm, SNR 11.0 dB | 9, −101 dBm, SNR 7.5 dB |

Native frames were received as well as text frames. The differences between
the two phases are one or two beacons per station, which samples of this size
cannot interpret. The lower signal at F4MLV-10 is a steady drift over the whole
morning, from −85 dBm around 07:30 to −105 dBm around 09:30, with no step at the
format change; F4MLV-2 and F4MLV-15 received a stable level at the same time.
F4MLV-10 relayed the native beacons, and F4MLV-15 uploaded them to APRS-IS
unchanged.

A T-Deck Mic-E beacon is 29 bytes native and 47 bytes as text; at SF12, 125 kHz
and CR 4/5 it lasts about 1.6 s instead of 2.3 s according to the airtime
formula. The trial says nothing about range at the edge of coverage: every link
had a comfortable margin.

F4MLV-10 was in eco mode for most of the trial, switched on and off by remote
commands around 07:12 and 10:05 UTC; both phases are affected in the same way.

## RXT trailers uploaded to APRS-IS by third-party iGates

A read-only APRS-IS listener with the filter
`d/F4MLV-10/F1ZDB-10/F4MLV-15/F4MLV-2` recorded, from 09:44:29 to 10:29 UTC,
the packets relayed by our digis that reached APRS-IS.

Until 10:03:54, 29 packets relayed by F4MLV-10 (15) or F1ZDB-10 (14) reached
APRS-IS. 28 still carried their RXT trailer, and all 28 were uploaded by iGates
that do not run this firmware: F4JQT-10 (8), F4KOL-4 (5), F1IXL-10 (5),
F4BPJ-10 (5), F5SPA-10, F4GCF-1, F4LBZ-10, F4INI-10 and F4GCF-10 (1 each). The
29th, relayed by F1ZDB-10 then by the third-party digi F4DMQ-2, ends with a
corrupted character where the trailer was.

After RXT was disabled on our digis during the morning, a single relay reached
APRS-IS until 10:29, at 10:07:15, uploaded by F4MLV-2 without trailer. A relay
identical to the original frame is dropped as a duplicate by APRS-IS, which is
consistent with this near absence.

On this network, where most iGates do not remove the trailer, RXT in the data
field therefore reaches APRS-IS for almost every relay heard by a third-party
iGate. Those copies are no longer identical to the others: they show up twice
on aprs.fi and, for a beacon carrying telemetry such as F6DEV-10's, they
trigger aprs.fi's "Duplicate telemetry sequence" error.

RXT was therefore disabled on our digis during the morning.

## RXT in a hidden block

The measurements were then moved out of the text: on air they follow the
packet after a zero byte and a marker byte. Ricardo's firmware reads the
received frame as a C string and stops at the zero byte; our stations read the
frame byte by byte and find the block (`docs/RXT.md`).

On the bench, F4MLV-15 was flashed with the upstream firmware (`fc256de`) and
F4MLV-2 with a test build adding a dummy block containing `>`, `:` and `{`.
The `NUL-T2` frame was received by F4MLV-15 with no trace of the block,
uploaded clean to APRS-IS as a single copy, and relayed without the block by
F4MLV-15 and by the third-party digi F5ZQC-10.

With the final firmware on F4MLV-15, F4MLV-2 read the F4MLV-15 measurement from
the hidden block, and a native T-Deck beacon relayed by F4MLV-15 gave the hop
`F4MLV-15<--F4MLV-7` with its measurements. Once F4MLV-10 and F1ZDB-10 ran the
same firmware with RXT enabled again, a single packet that went through our
digis showed up on APRS-IS between 14:17 and 14:57 UTC, clean; Graywolf showed
40 links at the same time, including measurements carried in the block.

The hidden block does not survive a third-party digi, which relays the text
only. In the F4MLV-10 SD log, 40 % of the receptions carrying measurements had
gone through one of our digis then a third-party digi: those measurements are
lost. Frames with several measurements, already rare (4 % of F4MLV-10's
relays), become exceptional.

## SNR range

The original RXT encoding limits SNR to −9..+12 dB. Out of 26,447 receptions at
F4MLV-10, 18.7 % were below −9 dB and 27.7 % of the transmitted measurements
sat at −9 dB. The range is now −24..+20 dB in 0.5 dB steps, which leaves 0.1 %
of the receptions out of range. All stations must use the same encoding; our
four stations were updated together on 30 September (build
`2026-09-30 15:05:06 UTC`).

## Next

RXT is enabled again on F4MLV-10 and F1ZDB-10, in the hidden block. The
APRS-IS listener on `firmin` keeps running to confirm the absence of
duplicates over a full day before publishing on the flasher. The hidden block,
the new SNR range and the loss of measurements after a third-party digi are to
be presented to Jon N7UV.

## Data

`logs/native-20260929/`: `pilot-tdeck.jsonl` (TNC and APRS-IS streams),
`F4MLV-10_sd_pilot.csv`, `F1ZDB-10_tnc_2026-09-30.log` and
`aprsis-relays-2026-09-30.log`. These files stay local.
