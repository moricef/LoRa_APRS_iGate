# RXT telemetry

## WebUI control

The **Enable RXT relay telemetry** switch controls whether this digi appends
its own RXT tuple to eligible packets that it relays over RF. It is enabled by
default to preserve the existing RXT firmware behaviour.

Turning the switch off does not disable RXT reception or decoding, the local
RXT dashboard, `/rxt.json`, or the removal of RXT trailers before packets are
uploaded to APRS-IS. APRS-IS cleanup is deliberately unconditional.

This branch integrates N7UV's experimental receive telemetry (RXT) with the
WIDE2 token fix, SD logging, APRS telemetry counters, MQTT, APRS-IS and the
TNC interfaces.

RXT is experimental. All participating stations must use compatible LoRa
settings because the TTH encoding scale depends on the configured bandwidth
and spreading factor.

## RF format

This is RXT v2 (proposal: `docs/RXT_V2.md`). An RXT-capable digipeater
appends one five-character tuple to a relayed APRS packet:

```text
SOURCE>DEST,PATH:payload{IRFSH}
```

The five printable characters encode, in order:

| Field | Meaning | Encoding |
| --- | --- | --- |
| I | Relay identity | Fingerprint of the callsign the digipeater writes into the path |
| R | RSSI | Linear, -130 dBm to -41 dBm, 1 dB steps |
| S | SNR | Linear, -24 dB to +20 dB, 0.5 dB steps |
| F | Frequency offset | Non-linear, approximately -2500 Hz to +2500 Hz |
| H | Time to hop (TTH) | Exponential, scaled from the active LoRa symbol time |

TTH covers the interval from completion of reception to completion of the
relay transmission. It therefore includes time spent in the output queue,
CAD/DIFS/backoff waiting and the transmitted packet's time on air.

The fingerprint is the 32-bit FNV-1a hash of the callsign in upper case,
without `*`, SSID included (the tactical callsign when one is configured),
taken modulo 89 plus 33. Test vectors are listed in `docs/RXT_V2.md`.

Each subsequent RXT-capable digipeater appends its tuple inside the same
braces, but only if the existing trailer passes the path check below. The
implementation retains at most three tuples (15 characters).

RXT is attached only to a genuine digipeated RF packet. Locally generated
beacons, APRS telemetry, query responses, APRS-IS-to-RF packets, MQTT input
and TNC input have no receive context and never receive an RXT tuple. APRS
messages, ACKs and REJs are also excluded, including when the message is
carried inside one or more third-party (`}`) frames.

## Per-packet measurements

RSSI, SNR, frequency offset and the reception-completion timestamp are copied
into the output queue together with the packet. Delayed packets therefore do
not reuse measurements from a later reception, and a beacon queued ahead of a
relay cannot erase its TTH origin.

## Path resolution and trailer check

The decoder considers path entries through the last starred entry to have
been used. This supports both conventions encountered on air:

```text
CALL1,CALL2*,WIDE2-1
CALL1*,WIDE2-2*,CALL2*,WIDE2-1
```

Entries after the last `*` are unconsumed and are not reported as hops.

A final `{...}` is treated as RXT only if it has 5, 10 or 15 characters in the
`!`..`z` range and every tuple ID matches the fingerprint of a used path
entry. The same check applies at both points where the firmware handles the
trailer:

- when relaying, a trailer that fails the check is left intact and a new
  `{...}` is opened after it;
- before APRS-IS, MQTT, TNC and the JSON stream, the final `{...}` is removed
  only if it passes the check.

Tuples are attributed in path order: each tuple goes to the next used entry
whose fingerprint matches its ID, so two relays sharing a fingerprint are
separated by their order. No whitelist of RXT-capable digipeaters is needed.

Legacy digipeaters may appear in the physical path but do not add a tuple.
TNC2 output reports these hops as `NA`.

A tuple whose ID matches no remaining path entry is still reported, under the
placeholder `RXT_NODE_n<--UNKNOWN`. The tuple is never silently discarded.

## Output boundaries

The RXT trailer is retained while a packet remains on RF so another
RXT-capable digipeater can append its measurement. It is removed before the
packet is sent to APRS-IS, MQTT, the WebUI map or a TNC client.

In TNC2 mode, clients receive the clean APRS packet followed by local receiver
metrics and decoded hop records. In KISS mode, clients receive only the
KISS-encoded APRS frame; textual metrics are never inserted into the binary
stream. The `tnc.protocol` setting controls both serial and TCP input/output.

## Web dashboard

The WebUI RXT panel keeps the last ten received frames carrying an RXT
trailer. It shows the clean APRS frame, the local receiver RSSI/SNR/frequency
offset, the raw tuple field and one decoded row per physical hop, including
`NA` for legacy hops and placeholder names for tuples whose ID matches no
path entry. The newest frame is shown first
and the panel refreshes every five seconds while it is open.

The same records are available from `GET /rxt.json`. Each record includes an
`age_ms` value measured from RF reception to the HTTP response. This monotonic
age lets polling clients distinguish a new observation from one of the same
ten retained records without depending on clocks or time zones. The JSON uses
explicit unit-bearing field names: local measurements are grouped under `local`, while
the decoded chain is returned as `rxt_hops` with `rssi_dbm`, `snr_db`,
`fo_hz` and `tth_ms`. The `packet` field is the clean TNC2 frame without its
RF-only RXT trailer; `rxt_raw` preserves the tuple characters separately.

## Binary packets in the APRS JSON stream

The versioned APRS JSON stream always preserves the authoritative clean packet
in `packet.raw_tnc2_base64` and the information field in
`packet.information.raw_base64`. The optional `packet.tnc2` and
`packet.information.text` projections are emitted only for valid UTF-8 without
ASCII control characters. A binary Mic-E DTI (`0x1c` or `0x1d`) is therefore
reported through `information.dti_hex` and the Base64 fields, without a
control byte in a JSON string. Printable DTIs also retain the optional
one-character `information.dti` projection.

## SD logging

The SD variant records RXT in `/aprs_rx.csv` using this schema:

```text
t_ms,event,rssi_dbm,snr_db,ferr_hz,tth_ms,rxt_rx_hex,rxt_tx_hex,tnc2
```

Normal receive rows contain the complete received RXT trailer in
`rxt_rx_hex`. Each successfully transmitted RXT relay adds an `RXT_TX` row
with the local receiver measurements, final TTH and local tuple in
`rxt_tx_hex`. Tuples are hexadecimal so every printable RXT character,
including commas and quotes, remains valid CSV. The TNC2 frame stays in the
last column because APRS frames can contain unquoted commas.

The logger writes a `# columns=...` marker after every `# boot`, allowing a
file created by an older firmware to continue with the new schema without
being mistaken for old six-column rows.

The log can be downloaded without removing the card, with the WebUI
credentials:

```text
GET /sd/log                  current file, aprs_rx.csv
GET /sd/log?file=old         rotated file, aprs_rx.old
GET /sd/log?tail=65536       last 65536 bytes of the current file
```

The size is fixed when the request starts. The file is read in chunks under
a mutex shared with the logger, so logging continues during a download; a
log line is dropped only if the card stays busy for more than 200 ms.

## APRS telemetry activity rates

The separate APRS encoded telemetry contains three rates normalized using the
actual elapsed time since the previous encoded telemetry report:

| Parameter | Meaning |
| --- | --- |
| RX_rate | LoRa APRS packets accepted by the receiver after blacklist filtering, in packets/hour |
| RelRate | Packets accepted and queued for digipeating, in packets/hour |
| DrpRate | Packets rejected by blacklist, duplicate, path, self or NOGATE rules, in packets/hour |

These are activity counters, not a delivery balance:

- With digipeating disabled (or RF transmission disabled), accepted receptions
  still increase `RX_rate`, without a relay or digi-rejection decision. They
  must not be counted as drops merely because the device is acting as an iGate.
- Blacklisted packets increase `DrpRate` before the receiver accepts them, so
  they do not increase `RX_rate`, even when digipeating is enabled.
- `RelRate` counts packets accepted and queued by the digi, not confirmed RF
  transmissions or successful delivery. Local ACKs/replies and APRS-IS-to-RF
  messages are not digipeated receptions and do not increase this counter.
- Each rate is independently rounded and capped at 8280 packets/hour.

Consequently, `RX_rate = RelRate + DrpRate` is **not an invariant**, including
in digi mode. A difference between these values is not by itself evidence of
packet loss. Locally handled queries are also counted as digi drops when they
reach the digi handler: they are consumed rather than relayed.

The raw counters reset after a report is generated. The transmitted hourly
rate saturates at 8280, the maximum value of the two-character base-91 field.
Using the real measurement duration also keeps manually requested or delayed
beacons comparable with regularly scheduled reports. The APRS metadata labels
the three values `RX_rate`, `RelRate`, and `DrpRate`, all with the `pkt/h` unit,
so APRS clients do not present counts accumulated over unlike intervals as if
they were comparable. These rates are currently emitted only when encoded
battery voltage telemetry is enabled, at least one voltage channel is selected,
and weather telemetry is inactive. They do not consume RXT tuples.

## Compatibility limitation

RXT v2 is not compatible with v1 (four-character tuples, whitelist-based
attribution). A v1 trailer fails the v2 check: it is neither decoded nor
removed, and a v2 relay opens a new trailer after it. All RXT-capable
digipeaters of a network must therefore be updated together.

The trailer check reduces, but does not remove, the risk of treating a user
comment as RXT data: a comment ending with 5, 10 or 15 characters in braces
whose tuple IDs happen to match used relays (about 1 chance in 89 per tuple)
is treated as RXT. For duplicate suppression, where no path is available,
only the v2 shape is checked.

## Configuration migration

Older default configuration images may omit `tacticalCallsign`. Assigning
that missing JSON property directly to the WebUI input can turn JavaScript's
`undefined` value into the literal text `"undefined"`; saving the form would
then make a digipeater replace a WIDE alias with `undefined*`.

The WebUI now treats a missing callsign property as an empty value. The
firmware also removes persisted `"undefined"` or `"null"` sentinel values at
startup and when saving the WebUI form. As a final RF safeguard, digipeating
is refused if the selected station identity is still empty or contains one
of those sentinels.

## Verification

Run the host protocol tests:

```sh
make -C test/rxt_host test
```

Build both tested firmware variants:

```sh
pio run -e ttgo-lora32-v21 -e ttgo-lora32-v21_SD
```

Host tests cover the fingerprint test vectors, the path check when relaying
and before APRS-IS, attribution in path order with colliding fingerprints,
the documented chance collision, trailer append/strip behavior, the
three-tuple cap,
multi-star paths, single-star paths, unused paths, commas in APRS payloads,
message detection through nested third-party frames, and safe text projection
for binary Mic-E packets. On 2026-09-25, F4MLV-15 received a controlled Mic-E
`0x1c` frame over RF and emitted valid JSON with byte-exact Base64 and
`dti_hex: "1c"`; Graywolf consumed the event and advanced its persisted
cursor. Hardware validation is still required for RF multi-hop RXT and both
TNC transport modes.
