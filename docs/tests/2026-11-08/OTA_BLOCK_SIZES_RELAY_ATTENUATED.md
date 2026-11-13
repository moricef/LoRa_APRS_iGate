# OTA block sizes over an attenuated relayed link — tests of 8 November 2026

[Version française](OTA_BLOCK_SIZES_RELAY_ATTENUATED_fr.md)

## Purpose and limits

This campaign compares six OTA block sizes over a degraded relayed link. Each
run transfers a **65,536-byte** sample, reads it back from the inactive
partition and verifies its SHA-256 hash. It does not validate a complete image
or its activation.

Sample SHA-256:
`d60ff2652f6fe489ae5d816451a5696d545c802565eb464438132aa4c0617252`.

## Setup

The route is **MLV-2 -> MLV-10 -> MLV-15**:

- MLV-2 is the controller. It uses the OTA bridge integrated into the OTA-A
  iGate firmware, with no separate bridge firmware;
- MLV-10 is the relay;
- MLV-15 is the target and carries the attenuation cascade;
- the control profile is 434.100 MHz, BW125, SF12, CR 4/5, 20 dBm;
- the transfer profile is 434.500 MHz, BW500, SF7, CR 4/5;
- no received image is activated.

The integrated bridge acted as the controller throughout the series. One fix
was required before the campaign: opening the serial port was resetting the
board.

## Attenuation and received level

The cascade consists of two sections using a total of two 820 ohm resistors
and four 51 ohm resistors. Its insertion loss measured with a NanoVNA is
**53 dB**. On the radio link, the observed difference was only **28 dB**;
these two figures must therefore not be treated as equivalent.

With MLV-15 set to 2 dBm, MLV-10 received it at **-113 dBm** with
**+3.75 dB SNR**. This condition is slightly worse than the SOMTNP to WHTANK
and SOMTNP to PINAL links described by N7UV.

## 2 dBm series

The 2 dBm series stopped during its first run. The link was lost after
**36,864 bytes** had been written. This run therefore provides no comparison
between block sizes.

## 9 dBm series

At 9 dBm, **21 of 22 runs** completed. Three full repetitions of all six block
sizes were obtained. A fourth partial repetition added three successful runs
before stopping on the 160-byte blocks.

The following durations are medians from the three complete repetitions:

| Useful block size (bytes) | Median for 64 KiB (s) |
|---:|---:|
| 128 | 135 |
| 175 | 137 |
| 160 | 144 |
| 96 | 190 |
| 64 | 228 |
| 34 | 370 |

Under these test conditions, block sizes from **128 to 175 bytes** produced
the fastest transfers. Blocks from 34 to 96 bytes provided no robustness gain
large enough to offset their overhead. The two-second difference between 128
and 175 bytes over three repetitions is not sufficient to rank them
definitively.

About half of the bench time was spent on roughly ten SF12 control exchanges
for each run. During an uninterrupted complete update this negotiation occurs
only once, so its relative cost is much smaller.

## Failures at 36,864 bytes

The two retained failures, one at 2 dBm and one at 9 dBm, followed the same
pattern and occurred after **36,864 bytes** had been written. Page-status
requests stopped receiving replies, the link was declared lost and the
recovery attempt failed. No board restart was observed.

### Observed recovery sequence

A review of the traces after the campaign invalidated the initial diagnosis
that an old nonce had been reused. In both failures, the bench already
performed this sequence:

1. it returned to the control profile and waited 65 seconds: the 60-second
   lease plus a five-second margin;
2. it sent a new `HELLO` at SF12 and received a reply containing a new nonce;
3. it sent `PREPARE_FAST` at SF12 and received its acknowledgement;
4. it switched the controller to BW500/SF7;
5. all eight `STATUS` attempts on the fast profile went unanswered;
6. all three `RETURN_NORMAL` attempts on that profile also went unanswered;
7. the controller then returned to the normal profile, but all four SF12
   `STATUS` attempts still went unanswered.

Recovery therefore failed after the session had been reopened correctly,
during confirmation of the return to the fast profile. The controller traces
cannot determine whether the target or relay failed to switch profiles,
whether the first `STATUS` did not reach them, or whether its reply was lost.

The final SF12 silence does not prove that the target remained on the fast
profile. The first normal-profile attempt began while its fast-profile lease
could still be active. When the lease expired, the firmware returned to its
normal profile, called `cancel()` and invalidated the nonce; later attempts
carrying that nonce were then ignored. Only the target console can establish
the timing of the profile switch and session expiry.

### Initial cause not established

The controller logs do not explain the initial radio loss:

- before the 9 dBm failure, page-status requests already required a second
  attempt every two or three pages, indicating gradual degradation rather
  than an abrupt interruption;
- after successful SF12 renegotiation, no BW500/SF7 frame passed, although
  that profile had worked a few minutes earlier;
- the identical 36,864-byte offset in both failures may be coincidental.

The available traces therefore cannot attribute the loss to block size, the
fast profile, the relay or the target.

## Required follow-up

Before another complete series:

1. reproduce only the loss and recovery instead of replaying the full block
   size matrix;
2. record the MLV-15 serial console at the same time, including received OTA
   operations and profile transitions;
3. retain the controller and relay traces so reception of `PREPARE_FAST`, the
   actual switch and reception of the first fast-profile `STATUS` can be
   distinguished;
4. change the recovery logic only after the failing step has been located.

The traces, CSV/JSON results and manifests are retained locally under
`logs/block-cascade-20261108/` and `logs/link-calibration-20261108/`.

The [targeted campaign of 11 November](../2026-11-11/OTA_PROFILE_RECOVERY_ATTENUATED_fr.md)
did not reproduce the fixed 36,864-byte offset. It demonstrated two complete
durable recoveries and located the remaining failures on fast-profile replies
in this attenuated setup.
