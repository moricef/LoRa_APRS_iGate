# A native frame format for LoRa APRS

## Why

LoRa APRS carries plain APRS text, inherited from a protocol designed for
1200 bd VHF more than 25 years ago. Two things follow from that.

The format is ambiguous. A comment can be read as altitude (`/A=`), as PHG,
as course/speed, or, since RXT, as relay measurements. Mic-E even hides data
in the destination field. Every decoder guesses a little differently.

The format is verbose. Positions are sent as ASCII text, the path as full
callsigns, and at SF12 every byte costs airtime on a channel shared by the
whole network.

LoRa APRS is young, its firmwares are open source and reflashable, and a few
projects cover most of the installed base. It is a much better place than VHF
to fix this.

## Principle

Keep standard APRS where it is shared, change it where it is not.

- On air, LoRa stations use a native format: typed, delimited fields, compact
  encoding, no guessing.
- Towards APRS-IS, the iGate translates each frame into standard APRS text.
  APRS-IS, aprs.fi and VHF users see nothing new.
- From APRS-IS to RF (messages, objects), the iGate translates the other way.

Current LoRa APRS frames start with the header `<\xFF\x01`. The native format
would use `<\xFF\x02`. Existing firmwares ignore it, and new ones decode both.

## What it brings

- A strict grammar: the free comment is explicitly delimited and never parsed
  for hidden data.
- Shorter frames, so less airtime per packet and a less loaded channel.
- Room for native extensions such as RXT, instead of appending them to the
  comment.
- Callsigns of any length, including alphanumeric SSIDs (`-MC`, `-GS`).

## The one hard constraint

The translation to APRS-IS must be fully deterministic. Two iGates receiving
the same frame must produce exactly the same text, byte for byte. Otherwise
APRS-IS no longer recognizes duplicates, and aprs.fi shows the same packet
twice, which is what RXT v1 trailers already cause today.

## What has to change

**Specification.** Header and versioning; encoding of every useful type
(position with course, speed and altitude, status, message, ack and
reply-ack, telemetry, weather, object and item, queries, third-party frames);
addressing and relay path (WIDEn-N equivalent, used-hop marker); native
extensions (RXT, delimited free comment); the deterministic translation to
APRS and the reverse translation; test vectors and a reference corpus.

**Shared codec library.** Encoding, decoding and translation both ways, in
embeddable C++ with host tests, used by both the iGate and the tracker.
Graywolf (Go) either ports it or relies on the same test vectors.

**iGate/digi firmware.** Receive both headers; relay using the native path
and RXT directly; deduplicate on the decoded frame; translate towards
APRS-IS (q-construct, RFONLY and NOGATE included) and from APRS-IS to RF
(messages, objects, learned return path, reply-ack); local replies (`?`
queries, acks, `!RC1` commands); own beacons and telemetry; every output
(KISS/TNC2 as translated text, MQTT, syslog, JSON stream, SD log, received
packets page, RXT table); a setting for the transmit format (legacy, native,
both).

**Tracker firmware.** Beacons and messages in the native format, reception of
both; conversations, acks and reply-ack, raw frames screen; application
bridges (Bluetooth/KISS TNC to APRSdroid) as translated APRS text; built-in
digipeater if enabled; a transmit-format setting.

**Graywolf.** Its LoRa TNC2 transport either decodes the native format itself
or receives frames already translated by the iGate; native encoding if it
transmits to LoRa directly.

**Transition and tools.** Dual-format handling for as long as legacy
equipment is on the air; web flashers and documentation for the three
projects; a command-line decoder, and capture tools comparing received frames
with their APRS translation.

**Outside our control.** Upstream CA2RXU firmware, which equips most of the
LoRa APRS installed base, and the other LoRa APRS firmwares in circulation:
without them, the native format stays limited to the networks that adopt it.

## How it could spread

A working pilot network convinces more than a specification. Jon's network in
Arizona already runs this fork and started RXT, so it is a natural place to
try. For it to spread, three things have to hold:

- nothing visible for anyone else: identical, standard APRS on APRS-IS, no
  duplicates or rejected packets on aprs.fi;
- measured and published gains: airtime saved, RXT attribution without a
  whitelist, no truncated comments, with figures from the real network;
- a risk-free upgrade: a new iGate reads both formats from day one, so it can
  be deployed without breaking trackers already in service.

RXT v2 is a reasonable first step in that direction: limited in scope, tested
on our two networks, and a way to check the method before proposing a full
format.
