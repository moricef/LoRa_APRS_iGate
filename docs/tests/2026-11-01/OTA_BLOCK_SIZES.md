# Variable OTA blocks: hardware test, 1 November 2026

[Français](OTA_BLOCK_SIZES_fr.md)

All five block sizes delivered the same **8,192-byte sample**, with SHA-256
verification of the prefix read back from the inactive partition.
A follow-up checks **128- and 175-byte blocks through one relay** using the
same sample. These tests validate the benchmark; they do not determine
the optimal size for a distant link.

## Versions and setup

- Branch `experiment/lora-ota`, commit `d2349cc`; target
  `ttgo-lora32-v21-ota-a`, bridge `bridge_fast`, client `block_bench.py`.
- Two TTGO LoRa32 V2.1 boards with SX1278 radios: MLV-2 as the USB/radio
  bridge, MLV-15 as the target. No intermediate relay.
- Normal profile: 434.100 MHz, BW125, SF12, CR4/5, 2 dBm.
- Transfer profile: 434.550 MHz, BW250, SF8, CR4/6, 3 dBm.
- Target Wi-Fi and access point disabled, checked over radio.
- Antennas and layout unchanged during the series; distance and attenuation
  not measured. No controlled attenuation or injected loss.

One run per size, shuffled with seed `20261101`:
128, 175, 96, 204, then 32 bytes. Each run starts from zero.
Flash pages remain 4,096 bytes for every block size.

SHA-256 of the common sample:
`e0a770d1726664bb14211e7f51e9aaef3fa39dfa5f6d151dc3d7bffda5030492`.

SHA-256 of the application binary running on the target:
`9582c8cbf85a939d0efcfbdb95933647040d41b90ee9593d5a3948461406dd63`.
The ELF fingerprint reported over radio matches the one in the saved binary:
`a01331388424317447a7005f59dbd8b2b78357ae88dba8a58d8a8086180e50b7`.

## Direct-link observations

| Data block (bytes) | Bitmap (bytes) | Duration (s) | Calculated aggregate airtime (s) | Retransmitted blocks | SHA-256 |
|---:|---:|---:|---:|---:|---|
| 32 | 16 | 45.84 | 41.49 | 1 | Match |
| 96 | 8 | 25.28 | 23.35 | 1 | Match |
| 128 | 4 | 22.02 | 20.47 | 0 | Match |
| 175 | 4 | 19.89 | 18.54 | 0 | Match |
| 204 | 4 | 19.39 | 18.12 | 0 | Match |

Timing starts before WINDOW_START and ends after verification in flash.
Initial negotiation, return to the normal profile and session closure are
outside this interval. Airtime adds the confirmed transmissions of the
bridge and target, including acknowledgements and repairs. It is calculated
from frame sizes and radio settings, not measured with an RF analyser.
Counter queries are excluded.

All five runs retained the same target boot identifier and reported no
radio driver transmission errors.

- The 96-, 175- and 204-byte blocks exercise shorter blocks at page ends,
  without allowing a block to cross a page boundary.
- The 204-byte blocks produce 255-byte radio frames, the direct-link limit.
- The 96-byte run repaired block index 21 of the first page.
- The 32-byte run repaired block index 77: the sixteen-byte bitmap was used
  to request a repair beyond the first 32 blocks. The RF cause of these two
  losses has not been established.

The sample verifier neither validated a complete firmware image nor
requested activation. Each success covers only the 8,192 bytes written and
read back, equivalent to two flash pages.

## Follow-up through MLV-10

Selected route: **MLV-2 → MLV-10 → MLV-15**. The relay runs the
`ttgo-lora32-v21-ota-relay` variant, rebuilt from `482c6a1`; the target,
bridge and sample are identical to the direct test. The transfer profile
remains 434.550 MHz/BW250/SF8/CR4/6, with requested power of 3 dBm.
Target Wi-Fi remains disabled.

Relay ELF fingerprint, checked through its diagnostic interface:
`08af58e8e0e37f237a402c6f58a6f2e971d8e75275da45199563f5da9191d9ce`.

One run per size, 175 then 128 bytes. Each run starts from zero and checks
the same SHA-256 of 8,192 bytes read back from flash. The relay cache
receives a page, retransmits it to the target, then returns its
acknowledgement. Blocks and acknowledgements travel over radio; management
Wi-Fi is used to read the relay counters.

| Data block (bytes) | Duration (s) | Calculated aggregate airtime (s) | Controller / relay / target TX | Controller block retransmissions | Local repairs | SHA-256 |
|---:|---:|---:|---|---:|---:|---|
| 128 | 50.30 | 47.14 | 68 / 72 / 4 | 0 | 0 | Match |
| 175 | 45.12 | 42.49 | 52 / 56 / 4 | 0 | 0 | Match |

Neither run reports a target or relay reboot or a radio driver TX error.
They verify 175-byte blocks with the relay envelope, producing 255-byte
frames, and shorter blocks at page ends. They do not exercise local
repairs, since none were needed.

MLV-10 is on the same site as the other boards, at the end of its mast.
This route through one relay is not a long-distance test and does not
characterise a degraded link. No complete image is activated.

## Airtime calculation correction

The airtimes in both tables replace the initial estimates. On SX1278 with
RadioLib 7.6.0, the BW125/SF12 → BW250/SF8 transition applied bandwidth
before SF. When changing SF, the library correctly disabled LDRO in the
hardware register, but retained an old value in the state used by
`getTimeOnAir()`. The counters therefore overestimated transmission time.

Corrected values are recalculated using the final profile, an eight-symbol
preamble, explicit header and enabled CRC. Frame lengths come from TX traces
and ACK formats; their counts and byte totals match target and relay
counters. The same calculation with stale LDRO state reproduces their raw
counters exactly for all seven runs. Original traces are retained unchanged.

The fix applies SF before bandwidth to refresh RadioLib's calculation state
as well. Target, relay and bridge builds pass, as do the 34 Python tests.
This fix was not flashed during these tests: its counters still need a
hardware check. The correction changes neither measured durations nor
verified SHA-256 hashes.

## Scope and next steps

The benchmark can now be used for the comparative series described in the
[evaluation protocol](../../proposals/LORA_OTA_BLOCK_BENCH.md).
The durations above describe seven individual runs, not a statistical
distribution. They do not justify changing the default block size.

Repeated comparisons at greater distances and under documented RF
conditions, both directly and through a relay, remain to be done. Small
blocks may become advantageous if long blocks require more repairs; these
tests do not establish that threshold. Twelve-byte bitmaps, recovery
after failures with variable blocks, and a full image using this new block
geometry were not validated by this test.

Traces, CSV/JSON results, binaries and fingerprints are retained in
`logs/block-smoke-20261101/` and `logs/block-relay-20261101/`, local
directories ignored by Git.
