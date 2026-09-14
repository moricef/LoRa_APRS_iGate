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
messages are also excluded.

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
section and loaded at startup. Matching is case-insensitive and ignores SSIDs,
so `F4MLV` and `F4MLV-10` are equivalent.

Legacy digipeaters may appear in the physical path but do not consume an RXT
tuple. TNC2 output reports these hops as `NA`.

If a valid tuple cannot be associated with a path entry because the whitelist
is missing or incomplete, TNC2 still reports its decoded values under the
placeholder `RXT_NODE_n<--UNKNOWN`. The tuple is never silently discarded.

## Output boundaries

The RXT trailer is retained while a packet remains on RF so another
RXT-capable digipeater can append its measurement. It is removed before the
packet is sent to APRS-IS, MQTT, SD logs, the WebUI map or a TNC client.

In TNC2 mode, clients receive the clean APRS packet followed by local receiver
metrics and decoded hop records. In KISS mode, clients receive only the
KISS-encoded APRS frame; textual metrics are never inserted into the binary
stream. The `tnc.protocol` setting controls both serial and TCP input/output.

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
multi-star paths, single-star paths, unused paths and commas in APRS payloads.
Hardware validation is still required for RF multi-hop RXT and both TNC
transport modes.
