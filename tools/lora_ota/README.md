# LoRa OTA client

This client uses the USB bridge integrated into the unified iGate OTA firmware.
It requires Python 3.10 or later and PySerial (`python3 -m pip install pyserial`).
No Wi-Fi or USB connection to the target is needed during the transfer.

Start with the [operator guide](../../docs/LORA_OTA_GUIDE.md) and the
[complete-cycle RF report](../../docs/tests/2026-11-14/FULL_RELAYED_OTA_CYCLE_VALIDATION_20261114_en.md).
The downloadable client bundle on the Web Flasher includes these Python modules.

Select the **target** board, irrespective of the controller or relay board:

| Target | `--board-id` |
|---|---|
| TTGO LoRa32 V2.1 | `0x54474732` (default) |
| Heltec WiFi LoRa 32 V3.2 | `0x484C5633` |
| QRP Labs LightGateway Plus 1.0 | `0x51524C50` |

Use the matching application firmware, not a merged flash image. All participants
need compatible firmware and matching radio profiles. Relayed operation also
requires a common route key, separately from the target command key. Provision
keys and authorised identities before deploying a board; the client does not
remotely install initial firmware, keys or a partition table.

Run `python3 send_integrated.py --help` from this directory for options.
Use `--windowed --block-size 128` for new transfers. Without `--no-activate`,
the client activates and confirms the image; `--exercise-confirm-timeout`
deliberately tests rollback before reactivation and confirmation.
`--replace-existing` explicitly discards the previous inactive candidate's
resume point. Do not use it when resuming the same image.

The TTGO/SX1278 full-cycle laboratory test used 160-byte blocks. The Heltec
and QRP Labs builds are compiled ports, not hardware-validated OTA results.
Keep board-specific first installations and OTA trials on the bench.
