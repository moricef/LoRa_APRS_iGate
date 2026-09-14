# RXT telemetry

This branch integrates N7UV's experimental receive telemetry (RXT) with the
WIDE2 token fix, SD logging, APRS telemetry counters, MQTT, APRS-IS and the
TNC interfaces.

RXT is experimental. All participating stations must use compatible LoRa
settings because the TTH encoding scale depends on the configured bandwidth
and spreading factor.

## RF format

An RXT-capable digipeater appends one four-character tuple to a relayed APRS
packet:

```text
SOURCE>DEST,PATH:payload{RFSH}
```

The four printable characters encode, in order:

| Field | Meaning | Encoding |
| --- | --- | --- |
| R | RSSI | Linear, -130 dBm to -41 dBm, 1 dB steps |
| S | SNR | Linear, -9 dB to +12 dB, 0.25 dB steps |
| F | Frequency offset | Non-linear, approximately -2500 Hz to +2500 Hz |
| H | Time to hop (TTH) | Exponential, scaled from the active LoRa symbol time |

TTH covers the interval from completion of reception to completion of the
relay transmission. It therefore includes time spent in the output queue,
CAD/DIFS/backoff waiting and the transmitted packet's time on air.

Each subsequent RXT-capable digipeater appends its tuple inside the same
braces. The implementation retains at most three tuples (12 characters).

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

## Path resolution and whitelist

The decoder considers path entries through the last starred entry to have
been used. This supports both conventions encountered on air:

```text
CALL1,CALL2*,WIDE2-1
CALL1*,WIDE2-2*,CALL2*,WIDE2-1
```

Entries after the last `*` are unconsumed and are not reported as hops.

`rxtWhitelist` is a space-separated list of digipeaters known to append RXT
tuples. It is configured in the WebUI under the station blacklist/manager
section and loaded at startup. Matching is case-insensitive but otherwise
exact, including the SSID when present. For example, `F4GCF-4` and
`F4GCF-10` are distinct stations. This prevents a tuple from being assigned
to another station sharing the same base callsign.

Legacy digipeaters may appear in the physical path but do not consume an RXT
tuple. TNC2 output reports these hops as `NA`.

If a valid tuple cannot be associated with a path entry because the whitelist
is missing or incomplete, TNC2 still reports its decoded values under the
placeholder `RXT_NODE_n<--UNKNOWN`. The tuple is never silently discarded.

## Output boundaries

The RXT trailer is retained while a packet remains on RF so another
RXT-capable digipeater can append its measurement. It is removed before the
packet is sent to APRS-IS, MQTT, the WebUI map or a TNC client.

In TNC2 mode, clients receive the clean APRS packet followed by local receiver
metrics and decoded hop records. In KISS mode, clients receive only the
KISS-encoded APRS frame; textual metrics are never inserted into the binary
stream. The `tnc.protocol` setting controls both serial and TCP input/output.

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

## APRS telemetry counters

The separate APRS encoded telemetry contains three interval counters:

| Parameter | Meaning |
| --- | --- |
| RX | Valid LoRa APRS packets accepted by the receiver |
| Relay | Packets accepted and queued for digipeating |
| Drop | Packets rejected by blacklist, duplicate, path, self or NOGATE rules |

Each counter saturates at 8280 and resets after an encoded telemetry report is
generated. These counters are currently emitted only when encoded battery
voltage telemetry is enabled, at least one voltage channel is selected, and
weather telemetry is inactive. They do not consume RXT tuples.

## Compatibility limitation

The wire format is kept compatible with N7UV's implementation. It has no
explicit marker other than braces: an ordinary APRS payload ending with 4, 8
or 12 printable characters enclosed in `{}` is indistinguishable from RXT.
Changing that format requires coordination between all RXT implementations.

An ordinary brace suffix of any other length is preserved when a new RXT
tuple is appended.

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

Host tests cover trailer append/strip behavior, the three-tuple cap,
multi-star paths, single-star paths, unused paths, commas in APRS payloads,
and message detection through nested third-party frames. Hardware validation
is still required for RF multi-hop RXT and both TNC transport modes.
