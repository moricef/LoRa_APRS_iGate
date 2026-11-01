# Variable OTA blocks: hardware test, 1 November 2026

[Français](OTA_BLOCK_SIZES_fr.md)

All five block sizes delivered the same **8,192-byte sample**, with SHA-256
verification of the prefix read back from the inactive partition.
This test validates the benchmark on a direct link. It does not determine
the optimal size for a distant or relayed link.

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

## Observations

| Data block (bytes) | Bitmap (bytes) | Duration (s) | Calculated aggregate airtime (s) | Retransmitted blocks | SHA-256 |
|---:|---:|---:|---:|---:|---|
| 32 | 16 | 45.84 | 41.70 | 1 | Match |
| 96 | 8 | 25.28 | 23.56 | 1 | Match |
| 128 | 4 | 22.02 | 20.63 | 0 | Match |
| 175 | 4 | 19.89 | 18.70 | 0 | Match |
| 204 | 4 | 19.39 | 18.28 | 0 | Match |

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

## Scope and next steps

The benchmark can now be used for the comparative series described in the
[evaluation protocol](../../proposals/LORA_OTA_BLOCK_BENCH.md).
The durations above describe five individual runs, not a statistical
distribution. They do not justify changing the default block size.

Repeated comparisons under documented RF conditions, followed by tests with
the intermediate relay, remain to be done. Twelve-byte bitmaps, recovery
after failures with variable blocks, and a full image using this new block
geometry were not validated by this test.

Traces, CSV/JSON results, binaries and fingerprints are retained in
`logs/block-smoke-20261101/`, a local directory ignored by Git.
