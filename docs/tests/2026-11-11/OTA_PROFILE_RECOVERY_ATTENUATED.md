# OTA recovery over an attenuated relayed link — tests of 11 November 2026

[Version française](OTA_PROFILE_RECOVERY_ATTENUATED_fr.md)

## Purpose

This campaign focuses only on the condition that interrupted two runs at
36,864 bytes on 8 November. It checks recovery of both the radio profile and
durable progress while recording the target's serial console throughout the
series.

Each run transfers a **65,536-byte** sample into the inactive partition, then
reads it back and verifies its SHA-256 hash. No image is activated.

## Setup

The route is **MLV-2 -> MLV-10 -> MLV-15**:

- MLV-2 is the controller and uses the bridge integrated into the iGate
  firmware;
- MLV-10 is the relay;
- MLV-15 is the target and retains the attenuation cascade used on
  8 November;
- useful blocks are 160 bytes long;
- the control profile is 434.100 MHz, BW125, SF12, CR 4/5; MLV-2 transmits at
  20 dBm, while MLV-10 and MLV-15 use their 2 dBm base profile;
- the transfer profile is 434.500 MHz, BW500, SF7, CR 4/5, 9 dBm;
- the temporary profile lease is 60 seconds.

Two bench defects were corrected before the series. The bridge serial port
now remains open after switching to 921,600 bit/s, avoiding a reset of CH343
boards. After a lost `RETURN_NORMAL` acknowledgement, the controller now
waits for the lease to expire, opens a new session with `HELLO`, then checks
status on the normal profile. This second correction was first reproduced
during a complete transfer and was then covered by a unit test.

## Results

Five runs were performed under the same conditions. Since the bench stops at
the first failure, their results are spread across four result directories.

| Run | Result | Duration | Observed link losses |
|---:|---|---:|---|
| 1 | 65,536 bytes verified | 251.6 s | recovery at 8,192 bytes |
| 2 | failure before the first page | - | recovery at 0, followed by no fast-profile `STATUS` replies |
| 3 | failure before the first page | - | no `STATUS` replies while entering the fast profile |
| 4 | failure after making progress | - | recoveries at 40,960 then 45,056 bytes; second fast-profile entry failed |
| 5 | 65,536 bytes verified | 362.9 s | recovery at 8,192 bytes |

Each of the two completed runs required five retransmissions from the
controller. The relay performed no local repair during run 1 and two during
run 5. Their measured cumulative radio airtime was 118.48 and 122.57 seconds,
respectively.

Durable recovery is therefore demonstrated twice: after a loss at 8,192
bytes, a new session found the pages already stored and the transfer reached
65,536 bytes with successful SHA-256 verification.

The fixed 36,864-byte offset observed on 8 November was not reproduced.
Losses in this series occurred at 0, 8,192, 40,960 and 45,056 bytes.

## Target observations

The MLV-15 console shows only one boot before the campaign and no reset during
the five runs. It also shows several `STATUS` operations being received and
processed even though their replies do not appear in the controller log. In
this setup, the BW500/SF7 return path is therefore the observed limiting
factor. The traces cannot attribute the loss to the relay, propagation or the
controller receiver.

The normal-profile return fix worked during all three failed runs. After the
lease expired, a new SF12 `HELLO` and `STATUS` received replies, allowing the
bench to close the session cleanly. It does not repair the loss of
fast-profile replies, which is a separate problem.

## Conclusion and limits

Two of the five runs completed. This attenuated condition is therefore too
unstable for an unattended automatic update, even though durable recovery and
return to the normal profile work.

The test covers a 64 KiB sample, one block size, one route and a laboratory
attenuation cascade. It measures neither the success rate for a complete
image nor that of a field link. The logs remain under
`logs/block-cascade-20261111-targeted/` and are not published.
