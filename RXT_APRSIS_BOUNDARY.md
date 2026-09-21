# RXT at the RF/APRS-IS Boundary

## Purpose

This note describes how RXT telemetry must be handled when an APRS packet
travels through both the LoRa RF network and APRS-IS. It also explains why an
RXT trailer leaked by one iGate can prevent normal APRS-IS duplicate
suppression and cause APRS.fi to report duplicate or out-of-order telemetry.

RXT is an RF-local extension. It is useful while a packet is travelling over
RF, but it must never be included in the packet uploaded to APRS-IS.

## Observed example

F1ZDB-10 sent the following telemetry packet directly to APRS-IS:

```text
2026-09-14 17:41:35 CEST
F1ZDB-10>APLRG1,TCPIP*,qAC,T2ROMANIA:=L8i]3NrQ>a !GLoRa APRS|"/%b!P!/!B|
```

Seventeen seconds later, the same packet arrived through the LoRa RF network,
after being relayed by F4JQT-4 and F4MLV-10 and uploaded by F4INI-10:

```text
2026-09-14 17:41:52 CEST
F1ZDB-10>APLRG1,F4JQT-4*,F4MLV-10*,qAR,F4INI-10:=L8i]3NrQ>a !GLoRa APRS|"/%b!P!/!B|{,N5H}
```

The source, destination, position, comment and Base91 telemetry sequence are
the same. The only addition to the information field is the four-character
RXT tuple:

```text
{,N5H}
```

APRS.fi consequently reported the second packet as:

```text
Duplicate telemetry sequence
Delayed or out-of-order packet (sequence number)
```

## Why the duplicate reaches APRS.fi

The direct Internet packet and the RF copy represent the same APRS event.
Without the RXT trailer, APRS-IS can recognise the later copy as a duplicate
despite the different transport paths.

If an iGate uploads the RXT trailer, however, the APRS information fields are
no longer identical:

```text
Direct:  ...|"/%b!P!/!B|
RF copy: ...|"/%b!P!/!B|{,N5H}
```

The changed information field prevents normal duplicate suppression from
treating them as the same packet. APRS.fi therefore receives both packets and
detects that their embedded telemetry sequence number is repeated.

The RXT tuple does not create a new APRS measurement. It only describes the RF
reception and relay. It must not make the relayed copy look like a new APRS
packet outside the RF network.

## Correct packet lifecycle

An RXT-capable system must process a packet in this order:

1. Receive the complete RF packet, including any existing RXT trailer.
2. Extract and decode the existing tuples for local display, TNC output or
   logging as required.
3. Keep the trailer attached to the RF packet that will be digipeated.
4. When performing a genuine RF relay, append the local four-character tuple.
5. Preserve the complete tuple chain for subsequent RF hops.
6. If the packet is uploaded to APRS-IS, strip the complete RXT trailer
   immediately before writing the packet to the APRS-IS connection.

The tuple must not be stripped as soon as the packet is received. Doing so
would prevent the next RXT-capable digipeater from extending the multi-hop
tuple chain.

Likewise, the tuple must not be retained until after the APRS-IS upload. The
RF-to-Internet gateway is the boundary at which RXT stops being part of the
transmitted frame.

The expected transformation at that boundary is:

```text
RF input:
F1ZDB-10>APLRG1,F4JQT-4*,F4MLV-10*:=L8i]3NrQ>a !GLoRa APRS|"/%b!P!/!B|{,N5H}

APRS-IS output:
F1ZDB-10>APLRG1,F4JQT-4*,F4MLV-10*,qAR,IGATE:=L8i]3NrQ>a !GLoRa APRS|"/%b!P!/!B|
```

## Firmware implementation

The corrected firmware removes RXT in
[`APRS_IS_Utils::buildPacketToUpload()`](../src/aprs_is_utils.cpp), after the
APRS-IS path has been constructed and immediately before the returned packet
is passed to `upload()`:

```cpp
String buildPacketToUpload(const String& packet) {
    int colonIndex = packet.indexOf(":");
    String packetToUpload = packet.substring(0, colonIndex);

    // Add qAR/qAO and the iGate callsign here.

    packetToUpload += checkForStartingBytes(packet.substring(colonIndex));

    // RXT is an RF-only extension. Remove it at the APRS-IS boundary.
    return LoRa_Utils::stripRxtTrailer(packetToUpload);
}
```

This location is intentional. The original packet remains available to the
digipeater path, while the APRS-IS copy is normalised before transmission.

The firmware also strips RXT before exposing a packet to other non-RF
consumers where appropriate. The raw and decoded values may still be retained
in dedicated local SD log fields; they must not be embedded in the APRS packet
sent to APRS-IS.

## Deployment requirement

Every iGate that can hear and upload packets from the RXT RF network must
implement this boundary rule. A single outdated iGate is sufficient to leak
the tuple and recreate the duplicate symptom.

The example ending in:

```text
qAR,F4INI-10:...{,N5H}
```

shows that F4INI-10 uploaded the packet with its RXT trailer still present.
F4INI-10 must therefore be updated, or its RF-to-APRS-IS upload path must be
changed to perform the same RXT stripping operation.

The gateways do not have to run identical firmware. They do, however, have to
follow the same interoperability rule:

> Preserve RXT across RF hops; remove RXT at every RF-to-APRS-IS boundary.

Updating only the RXT digipeaters is not sufficient. All iGates within RF
coverage that may receive an RXT packet must also be compliant.

## Alternatives and their trade-offs

### `RFONLY`

The originating station can add `RFONLY` to its RF path, for example:

```text
WIDE2-1,RFONLY
```

The packet can still be relayed over RF, but compliant iGates will not upload
it to APRS-IS. This avoids the duplicate, but it also removes the RF network as
an APRS-IS fallback when the originating station's direct Internet connection
fails.

`RFONLY` is therefore an optional network policy, not a substitute for correct
RXT boundary handling.

In this firmware, `NOGATE` must not be used as an equivalent solution because
it is rejected by the digipeater code and stops the RF relay as well.

### Cross-network duplicate cache

An iGate could maintain a short-lived cache of packets recently received from
APRS-IS and suppress an RF upload when the same source and RXT-free information
field were already observed.

This is more complex and depends on the iGate receiving the direct APRS-IS
copy in time. It can be useful as an additional safeguard, but it is not needed
to solve the RXT leak itself.

### Pure digipeater operation

Changing the originating station or relay into a pure digipeater would also
remove one Internet path, but this sacrifices functionality unnecessarily.
Correct stripping at the iGate boundary allows simultaneous Internet and RF
operation without publishing RXT as part of the APRS payload.

## Relationship to the `undefined*` defect

The `undefined*` path defect and the APRS-IS duplicate problem are separate,
although both were exposed by RXT traffic.

Older configuration images could omit `tacticalCallsign`. The WebUI could then
save JavaScript's `undefined` value as literal configuration text, causing a
digipeater to replace a WIDE alias with `undefined*`.

Commit `a027a88` (`Prevent undefined digipeater identities`), created on
2026-09-14 at 14:16:55 CEST, added three safeguards:

- missing WebUI callsign values are converted to empty strings;
- persisted `undefined` and `null` sentinel values are repaired during
  configuration loading and saving;
- RF digipeating is refused if the selected station identity is empty or
  contains one of those sentinel values.

The corrected firmware was published at approximately 14:17 and flashed onto
F4MLV-10 and F4MLV-2 at approximately 14:19. F6DEV-10 still had the older
firmware but had been switched off since approximately 12:30. The last supplied
`undefined*` observation was at 12:47:15, and no recurrence was observed in the
later supplied traffic.

The available APRS-IS packets cannot identify which individual relay produced
`undefined*`, because the invalid value replaced the very identity needed for
that attribution. Individual attribution is not required to validate the
firmware-level fix: every affected digipeater should be upgraded before being
placed or returned to service.

## Verification procedure

After updating all relevant iGates:

1. Send a beacon from a station that publishes directly to APRS-IS and also
   transmits the same packet over RF.
2. Confirm locally that the RF copy contains its RXT tuple chain.
3. Confirm that each RXT digipeater preserves existing tuples and appends its
   own tuple only for a genuine RF relay.
4. Inspect the final iGate's serial or SD diagnostics and verify that the RXT
   data was received and decoded.
5. Inspect the packet actually uploaded by the iGate and verify that it ends at
   the original APRS payload, without an RXT `{....}` suffix.
6. Confirm that APRS.fi no longer displays a second telemetry packet carrying
   the same sequence solely because an RXT trailer changed its information
   field.
7. Repeat the test through every iGate capable of hearing the RXT network. One
   untested or outdated gateway can still reproduce the leak.

The required invariant is:

```text
RF relay path: RXT retained and extended
APRS-IS upload: RXT removed
```
