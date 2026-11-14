# Complete relayed OTA cycle: test of 14 November 2026

[Version française](VALIDATION_CYCLE_OTA_COMPLET_20261114_fr.md)

## Purpose and setup

Verify a complete application transfer, activation, rollback without
confirmation, reactivation and radio confirmation on physical boards.
This extends the 13 November transfer without activation. It used the Python
client, not a graphical interface.

Route: **F4MLV-2 -> F4MLV-10 -> F4MLV-15**, three TTGO LoRa32 V2.1/SX1278
boards. The controller uses the USB bridge integrated into the unified iGate
OTA firmware. Relay and target use the same firmware with configured roles.
The client does not open target USB. Target Wi-Fi/AP were disabled; radio
queries confirmed its offline state before and after the update.

| Parameter | Control | Temporary transfer |
|---|---|---|
| Frequency | 434.100 MHz | 434.500 MHz |
| LoRa | BW125 / SF12 / CR 4/5 | BW500 / SF7 / CR 4/5 |
| Power | 20 dBm | 9 dBm |

Uncompressed image, **160-byte payload blocks**, 4,096-byte pages. This test
validates the complete cycle at 160 bytes, not an optimum block size.

The cascade on F4MLV-15 uses two 820 ohm and four 51 ohm resistors. Its
NanoVNA insertion loss is **53 dB**, whereas the **observed difference over
the radio link was 28 dB** during the 8 November characterisation. These
figures are not interchangeable.

## Versions and result

- Initial: `4.0.2RXT-OTA`, build 11 November 2026, 15:00:28 UTC.
- Candidate: `4.0.2RXT-OTA-B`, non-diagnostic variant
  `ttgo-lora32-v21-ota-candidate-b`.
- Image: **1,782,112 bytes**, SHA-256
  `a0011d32ee6aa92a70b1185707fd14886adad032bceea24bf748f30e3f925517`.
- Transfer and verification: **3,762.66 s**, approximately **474 useful
  bytes/s**, including exchanges and repairs.
- Repairs: **173 blocks requested** across 165 `repair_page` events.
- Retries: **17 request timeouts**, not 17 complete pages retransmitted.
- No session loss or recovery; exit code **0**.
- Entire cycle: **4,264.53 s**, route selection to final session cleanup.

The target finishes on B, active and boot partition `0x1f0000`,
`pending=false`, with the expected image hash read through a fresh session.

## How the test was performed

The tested client used base `834bf7c` with local changes: integrated bridge
startup/shutdown, explicit candidate replacement and final readback. These
are now retained in commit **`6a9dc46072d35bbe11a68d4c1d53b515b3668135`**,
which also adds explicit target-board selection; the TTGO value used here is
unchanged. This commit preserves the client, not a bit-identical firmware
rebuild: use the archived image and hash rather than a later rebuild.

Requirements: Python >= 3.10 with PySerial; preinstalled OTA firmware and
compatible bootloader/partitions; target command key and authorised controller;
a separate common route key installed in relay/target and supplied to the
host. Reuse provisioned keys. Keep a persistent OTA counter without resetting
the radio anti-replay state.

Configure roles with authenticated APRS commands: `OTA=RELAY,F4MLV-15` to
the relay and `OTA=VIA,F4MLV-10` to the target. Read them back using
`OTA=ROUTE?`. Both boards initially listen on the control profile and the
relay does not beacon during transfer. Close controller serial monitors and
leave the cascade fitted.

Contents of `profiles.json`:

```json
{
  "normal": {"frequency_hz":434100000,"bandwidth_hz":125000,"spreading_factor":12,"coding_rate":5,"power_dbm":20},
  "transfer": {"frequency_hz":434500000,"bandwidth_hz":500000,"spreading_factor":7,"coding_rate":5,"power_dbm":9}
}
```

Equivalent historical invocation, with paths relative to the repository and
operator-supplied private files. **It activates the firmware and first
destroys the previous inactive candidate's resume point.** It retains the
historical 160-byte setting.

```bash
cd /path/to/LoRa_APRS_iGate
ROOT="$PWD"
BRIDGE=/dev/serial/by-id/CONTROLLER_USB_PORT
IMAGE="$ROOT/logs/ota-full-cycle-prepared-20261114/firmware-4.0.2RXT-OTA-B.bin"
TARGET_KEY="$ROOT/private/target-key.base64"
ROUTE_KEY="$ROOT/private/route-key.base64"
COUNTER="$ROOT/private/ota-counter"
PROFILES="$ROOT/private/profiles.json"
RUN="$ROOT/logs/ota-full-cycle-$(date -u +%Y%m%d-%H%M%SZ)"
mkdir -p "$RUN"
sha256sum "$IMAGE" > "$RUN/image.sha256"
python3 tools/lora_ota/send_integrated.py \
  --bridge "$BRIDGE" --source F4MLV-2 --target F4MLV-15 --via F4MLV-10 \
  --key-file "$TARGET_KEY" --route-key-file "$ROUTE_KEY" \
  --counter-file "$COUNTER" --board-id 0x54474732 \
  --negotiate-profile --radio-profiles "$PROFILES" \
  --windowed --block-size 160 --require-offline --replace-existing \
  --exercise-confirm-timeout --image "$IMAGE" --log "$RUN/transfer.jsonl" \
  > "$RUN/console.log" 2>&1
rc=$?
printf '%s\n' "$rc" > "$RUN/exit-code"
```

No `--compress` option is supplied. `--exercise-confirm-timeout` withholds
confirmation for 190 s after observing the candidate boot, exceeding the
firmware's 180-second deadline. The client checks rollback, reactivates B,
confirms twice to check idempotence, then reads version/partition/SHA/pending
through a fresh HELLO session.

## Evidence and limits

UTC timeline; line numbers refer to `transfer.jsonl`:

| Time | Line | Event |
|---|---:|---|
| 10:15:26.227 | 8 | Previous inactive candidate discarded |
| 10:15:57.577 | 14 | Target offline confirmed |
| 10:16:18.382 | 19 | Fast profile confirmed, 60-second lease |
| 11:19:01.343 | 24852 | Full image verified, status 0 |
| 11:19:19.741 | 24857 | Return to control profile confirmed |
| 11:20:03.475 | 24864 | B at `0x1f0000`, pending true |
| 11:23:30.836 | 24869 | Rollback to initial version at `0x10000` |
| 11:24:15.901 | 24875 | Second boot of B, pending true |
| 11:24:59.369 | 24880 | B confirmed, pending false |
| 11:25:13.385 | 24883 | Target still offline |
| 11:25:48.706 | 24889 | Fresh-session expected version, partition and SHA |

Private logs: `logs/ota-full-cycle-20261114-101435Z/`. Historical profiles:
`logs/block-cascade-20261108/profiles-transfer-9dbm.json`. SHA-256 hashes:

| File | Hash |
|---|---|
| `transfer.jsonl` | `0e23615d0d0710b8dbfd9aaf48cc49104772702388b6ff341ad16eb0ba0bd329` |
| `console.log` | `b141a6d4643df3510f93feff9b751a435a1d410f6206cc22d0aaef9c53d735b0` |
| Profile file | `9696fb33dd72c5f2f4d58eaeefb64ea8540b8325923ee35e73c6c69add6d7410` |

The log observes the confirmation timeout outcome, not the precise rollback
instant. It records decoded authenticated replies at the controller, not every
RF byte or target-console output; it cannot independently recheck every HMAC
or locate every loss.

This is one laboratory run through one relay to one target, not field
validation, a power-loss-during-activation test, or hardware evidence for the
Heltec/QRP Labs ports. Private keys and configuration backups are not
publication assets.

