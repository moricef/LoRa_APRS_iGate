# A native frame format for LoRa APRS

LoRa APRS currently carries APRS packets as text. This representation supports
interconnection with APRS-IS but retains constraints inherited from AX.25
addressing and APRS syntax. A native LoRa format can use explicit fields and
compact encodings while preserving translation to standard APRS.

This document describes the target architecture. The implemented draft-0
layout is documented in [LORA_APRS_NATIVE_FRAME_DRAFT0.md](LORA_APRS_NATIVE_FRAME_DRAFT0.md);
it does not yet implement a separate binary layout for every packet type
listed below.

## Limitations of the current representation

APRS combines structured data with comment text. Markers such as /A= and
PHG, and the rules for course and speed, require interpretation of the
information field. Mic-E also uses the destination field to encode data.
Implementations can interpret the same bytes differently.

RXT v1 introduced an additional ambiguity by appending receiver measurements
to the comment. A user comment ending in the same pattern can be mistaken
for RXT.

Text also consumes airtime. Source, destination, path and information fields
are transmitted on each packet; long comments and paths increase channel
occupancy, especially at SF12.

## RF encoding and frame version

The proposed format separates addresses, relay state, application data,
free text and RXT measurements. Position, course, speed, altitude, weather,
telemetry and messages should have defined representations. Free text should
have an explicit boundary rather than supply additional fields inferred
from comment markers.

Native addressing should support longer callsigns and alphanumeric SSIDs
such as -MC and -GS, without inheriting the AX.25 address-field limits.
The native specification must define maximum lengths. Gateways must handle
the separate limits of APRS-IS and AX.25 explicitly: an identity accepted on
LoRa is not necessarily representable on those interfaces.

The current text prefix is 3C FF 01. Draft 0 uses 3C FF 02. Receivers must
dispatch packets by frame type and discard unsupported types before text
parsing. Equipment without a native decoder cannot receive or relay native
traffic.

## Translation to and from APRS-IS

The iGate translates supported native packets into standard APRS text before
uploading them to APRS-IS. For downlink messages and objects, it translates
APRS text into the RF format selected for transmission.

APRS-IS and existing APRS applications continue to receive standard packets.
Native identities or fields that cannot be represented require an explicit
gateway policy; they must not be silently truncated or mapped by individual
implementations.

## Deterministic translation

Given the same native packet, independent decoders must recover identical
APRS source, destination and information bytes. APRS-IS uses these fields
for duplicate detection; the outer transport path may differ between iGates.

RXT is RF metadata and must remain separate from the APRS information field
sent to APRS-IS. Visible RXT trailers demonstrate why this matters: a
trailer retained by one iGate and removed by another produces different
information fields.

The specification must define exact translation rules and reference vectors
for encoding, decoding and gateway output.

## Application data and relay state

The target scope includes:

- position, course, speed and altitude;
- status;
- messages, ACK and reply-ACK;
- telemetry;
- weather;
- objects and items;
- queries;
- third-party frames;
- relay paths and RXT measurements.

Relay state should distinguish requested relays, consumed hops and remaining
hops explicitly. Relays should be able to update this state without
reconstructing it from textual WIDEn-N rewrites.

## Shared codec and conformance tests

An embeddable C++ codec can serve both the tracker and the iGate.
Independent implementations, including Go code where needed, must pass the
same reference vectors.

Tests must cover exact round trips, malformed input, unsupported versions,
address limits, path updates and the association of RXT measurements with
their relays. A corpus of captured frames provides additional compatibility
cases.

## iGate and digipeater integration

The iGate and digipeater firmware must:

- receive legacy text and supported native frame types;
- update relay state and retain each RXT measurement's association;
- deduplicate using the decoded APRS content;
- translate traffic to and from APRS-IS, preserving RFONLY and NOGATE policy;
- preserve messaging, ACK, reply-ACK and learned return-path behaviour;
- preserve local queries and authenticated remote commands;
- support locally generated beacons and telemetry;
- provide compatible TNC2, KISS, MQTT, syslog, JSON, SD-log and WebUI output.

KISS clients expecting AX.25 require AX.25 frames; TNC2 clients require
text. Neither interface can represent every proposed native identity.
Those limits must be documented and enforced at conversion boundaries.

Transmission settings must distinguish legacy, native and dual-format
operation. Sending both formats increases channel occupancy and is a
transition or test mode rather than an airtime-saving mode.

## Tracker integration

The tracker must receive both formats and support native beacons and
messages while preserving conversation handling, ACK and reply-ACK.

Interfaces to applications such as APRSdroid must continue to supply the
representation expected by that interface. If the tracker's digipeater is
enabled, it must also update relay state and preserve RXT associations.

## Graywolf integration

When an iGate supplies translated TNC2 or JSON, Graywolf does not need the
RF codec. A direct RF implementation would require native encoding and
decoding.

The interface specification must distinguish the APRS packet, local receiver
measurements and measurements reported by remote relays.

## Compatibility and deployment

Legacy and native formats must coexist during deployment. Receiving legacy
traffic must remain possible after a firmware update, but native
transmission requires compatible receivers and relays along the RF path.

Adoption depends on upstream firmware and other implementations outside this
fork. Tests must include mixed networks and verify that gateways produce
consistent standard APRS output.

## Pilot validation

The local network and Jon N7UV's Arizona RXT network provide potential
test environments. Native support and deployment must be established for
each participating station rather than inferred from RXT support.

Pilot measurements should cover:

- frame sizes and airtime by radio profile;
- delivery, relay behaviour and messaging;
- RXT attribution, including missing measurements and fingerprint collisions
  when converting to text;
- preservation of comment bytes;
- identical gateway output from independent receivers;
- coexistence with legacy equipment;
- processing time and memory use on the target boards.

The specification, test vectors, capture tools and firmware instructions
must identify the format revision tested.
