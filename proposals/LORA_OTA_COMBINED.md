---
title: "Firmware updates over LoRa — a combined approach"
subtitle: "Proposal for discussion, 7 November 2026"
lang: en
geometry: margin=2cm
---

## Summary

Three directions are on the table: a tested page transfer through a relay, delta
updates with a split firmware layout, and campaigns run either automatically or
step by step by an operator. They do not compete. They are layers of one
system, built on the same target behaviour: check a signed package, accept a
signed profile switch, receive, verify, activate, confirm.

One decision cannot wait: the partition table. It cannot be changed over the
air, so the layout installed at the next site visits is the layout those sites
keep.

## What is tested today

Bench, TTGO LoRa32 V2.1 with SX1278, controller → one relay → target, same site.

| Function | Status |
|---|---|
| Transfer in 4 KB pages, bitmap acknowledgements, repair cache in the relay | Validated: complete image through one relay |
| Resume from flash after a target reset, a relay restart or a power cut | Validated on short transfers |
| Temporary fast profile (BW500/SF7) by signed command, automatic return when its lease expires | Validated |
| Activation as a separate signed command, radio confirmation after boot, rollback otherwise | Validated (uncompressed images) |
| Relay and target routes set by signed APRS commands (`OTA=RELAY,<target>`, `OTA=VIA,<relay>`) | Validated: both routes set by radio, routed transfer started with the new route key, 241 KB written then interrupted on purpose |
| Image installed locally (web page or USB) accepted at boot; radio-activated images still wait for the radio confirmation | Validated on the relay board |
| Compressed pages: each 4 KB page deflated on its own, decoded on the target before writing | 64 KB sample through the relay, decoded SHA-256 verified on the target; full image 27% smaller (1,298,854 instead of 1,778,688 bytes) |

Not tested: activation of a compressed image, ESP32-S3 / SX1262 builds (they
compile), more than one relay, range, a busy APRS channel.

## Layers of the combined approach

| Layer | Proposal | Status |
|---|---|---|
| Transport to one station | The page transfer above | Tested on the bench |
| Payload | Delta (detools/bsdiff, then xz with an 8 KB dictionary) for the application; codec and model sent whole; compressed pages as the fallback for a complete image | Delta sizes measured on real builds; not run on a board |
| Firmware layout | Application, model and codec in separate partitions, each with an A and a B copy | To be agreed |
| Authorisation | Packages signed by the operator; profile switches requested only by authorised distributors | To be designed |
| Several stations at once | Broadcast with forward error correction, beside the acknowledged transfer | Later |

### Measured patch sizes

| Change between two builds | Patch | Tool |
|---|---:|---|
| Web page label only | 46.5 KB (2.6% of the image) | xdelta3 |
| Page decompression added | 203 KB (11.3%) | xdelta3 |
| 17 lines added | 20.6 KB | detools + xz |
| Whole-text PPM to LACF 2.0 | 302 KB | detools + xz |

Most of a patch comes from code that moves: the same source built twice differs
by 495 bytes. Keeping the model and the codec outside the application is what
keeps format changes small.

## Two ways to run a campaign

Both use the same firmware. Only the sender of the commands changes.

**Piloted.** The operator updates a first digipeater, then tells it to serve a
station it hears well and the operator does not. During the transfer the
serving digipeater returns briefly to the normal profile to report progress,
repairs and errors. The operator decides each next step. When both stations
use the same board, the serving digipeater can send its own confirmed image
from its running partition, with no extra storage.

**Automatic.** An iGate fed by MQTT or Wi-Fi receives a signed package and a
campaign: stations, time windows, and SNR thresholds taken from RXT tuples and
the neighbour table (SF7, SF9 or SF10 depending on the link). It proceeds on
its own and reports.

Safeguards for automatic campaigns:

- each station opts in by a setting;
- a limit on the number of digipeaters off the APRS channel at the same time;
- stop at the first error;
- operator pause and abort at any time;
- the boot confirmation and rollback stay as they are.

### Two separate rights

Providing a package (signed by the operator) and requesting a profile switch
(authorised distributors) are separate. A digipeater or an iGate can then serve
an image without holding the target's secret, and without being able to
produce one.

## The partition table

Proposed application slots are 1,664 KB each. The current update-capable build
is 1,785,765 bytes, so it fits only once the model and the codec are moved out
of the application; the application alone is estimated, not measured, at about
1,450 KB. The sizes, the model limit and the module interface have to be agreed
before the first release that uses them, and before sites that will be out of
reach receive firmware.

## Proposed order

1. Agree the partition layout and the model limit.
2. Signed package manifest; `HELLO` reports the SHA-256 of the running
   application, so a delta can check its base.
3. Delta applied in order into the inactive slot, on the page transport, with
   resume by patch position.
4. Piloted store-and-forward, with status reports on the normal profile.
5. Broadcast with forward error correction, and automatic campaigns.

## Open points

- Partition sizes, model limit and the codec module interface.
- Signature scheme and key handling.
- Status report interval during a transfer, and its cost in transfer time.
- SNR thresholds for choosing SF7, SF9 or SF10.
- Field trials: accessible sites first, never a site that will be out of reach.
