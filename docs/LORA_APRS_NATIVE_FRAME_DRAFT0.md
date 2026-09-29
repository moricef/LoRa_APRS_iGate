# Native LoRa APRS frame — draft 0

This is the byte layout implemented in `src/native_aprs.cpp`. It is a
working draft for discussion, not an agreed format. The reasons for a native
frame are in `LORA_APRS_NATIVE_FORMAT.md`.

## Principle

A native frame carries an ordinary TNC2 packet in binary form. Decoding it
gives back the original packet byte for byte, so an iGate uploads exactly
what a text frame would have produced and APRS-IS can still recognise
duplicates. Anything the draft cannot represent exactly travels as text.

The encoder checks this itself: it decodes every frame it builds, and if the
result differs from the input it falls back to carrying the whole information
field as text.

On the local corpora (26,303 receptions at F4MLV-10 and 1,212 RF frames from
the N7UV feed), every packet round-trips exactly. Airtime drops by 25.3 %
(SF12, CR 4/5) and 21.9 % (SF7, CR 4/6).

## Frame

```
'<' 0xFF 0x02 | header | source | destination | path... | [RXT] | [position] | text
```

The prefix differs from the text frame (`<` `0xFF` `0x01`) only in its last
byte. The current upstream firmware checks the prefix and ignores native
frames. Builds of this fork without native support do not check it: read as
text and cut at the first zero byte, 88 % of the corpus frames have no `>` and
are dropped silently; the other 12 % give a sender that is not a callsign, so
they are neither relayed nor uploaded, but can leave an unreadable line in the
SD log, the WebUI list and the JSON stream.

Multi-byte integers are big-endian.

### Header byte

| Bits | Meaning |
| --- | --- |
| 7–4 | number of path elements, 0 to 15 |
| 3 | an RXT block follows the path |
| 2 | the text is Huffman coded |
| 1–0 | information kind: 0 raw, 1 position |

## Address elements

Source, destination and each path element use the same encoding, chosen by
the first byte.

| First byte | Element | Following bytes |
| --- | --- | --- |
| `0 s nnn NNN` | `WIDEn-N`, with `*` if s = 1; N = 0 means `WIDEn` without `-N` | none |
| `1 0 s xxxxx` | callsign of 1–6 characters `A-Z0-9`, `*` if s = 1 | 4 |
| `1 1 lllll` | any other element, as text | l (1–31) |

For a callsign, the 4 bytes are the 6 characters, padded with spaces, in base
40 with the alphabet `␠A–Z0–9` (space = 0, A = 1, 0 = 27). `xxxxx` is the
SSID: 0 means none, 1–30 is the SSID, 31 means the SSID (31–255) is in one
extra byte after the 4 bytes. An SSID with a leading zero (`-0`, `-05`) is
carried as text.

Elements that are not in these forms are carried as text: alphabetic SSIDs
(`F4MLV-MC`), lower case, or malformed paths seen on the air (`WIDE2*2`,
`F6DEV-10**`).

## RXT block

Present when header bit 3 is set.

```
mask | tuple for the lowest set bit | tuple for the next | ...
```

Bit k of the mask means that path element k (0 = first element after the
destination) carries a measurement. Each tuple is 4 bytes: RSSI, SNR, FO and
TTH, each being the RXT v1 character minus 33. Only the first 8 path elements
can carry a tuple.

The measurement is tied to its relay by position in the structure. There is
no ID character and no braces, so it cannot be mistaken for a comment. The
block costs 1 + 4n bytes for n tuples, against 2 + 4n for the v1 trailer and
2 + 5n for v2.

## Position

Used for information fields starting with `!`, `=`, `/` or `@` whose layout
the draft can reproduce exactly. The flags byte comes first.

| Bit | Meaning |
| --- | --- |
| 1–0 | data type: 0 `!`, 1 `=`, 2 `/`, 3 `@` |
| 2 | timestamp present (3 bytes) |
| 3 | compressed position (else uncompressed) |
| 4 | course/speed present (3 bytes, uncompressed only) |
| 5 | altitude present (4 bytes) |
| 6 | compressed: course/speed bytes are two spaces |

Then, in this order:

- **Timestamp**: 6 digits × 4 + format, format 0 `z`, 1 `h`, 2 `/`.
- **Compressed position**: latitude and longitude as their base 91 values,
  27 bits each, packed into 7 bytes (latitude first); symbol table byte;
  symbol code byte; then either the T byte (bit 6 set) or the three `c s T`
  bytes.
- **Uncompressed position**: 6 bytes holding, from the high bits,
  latitude in hundredths of minute (20 bits) and a south bit, then
  longitude in hundredths of minute (21 bits) and a west bit; symbol table
  byte; symbol code byte. Positions with ambiguity (spaces instead of
  digits) are carried as text.
- **Course/speed**: course × 1024 + speed, 3 bytes, for a `ddd/ddd` extension
  right after the position.
- **Altitude**: offset of `/A=` in the comment (1 byte), then the altitude in
  feet as a signed 24-bit value. The decoder puts `/A=` back at that offset,
  6 digits, or `-` and 5 digits.

The comment that remains, without `/A=`, is the text.

## Text

The rest of the frame. If header bit 2 is clear, it is copied as is. If it is
set, one byte gives the length of the text, followed by the text coded with a
canonical Huffman code, padded with zero bits to a whole byte.

The code lengths are fixed and part of the format:
`include/native_aprs_huffman_table.h`, 256 entries, 15 bits at most. Codes are
assigned in order of (length, byte value). The encoder only uses Huffman when
the result, including the length byte, is shorter than the text.

The table was built from the text of both corpora. Changing it changes the
format.

## Test vectors

`test/native_aprs_host/vectors.txt` holds 54 cases from the corpora and a few
built by hand. Each line gives the RXT tuples, the TNC2 packet in hex and the
expected native frame in hex. Any other implementation should reproduce them
exactly.

## Firmware support

The iGate receives native frames: it decodes them to TNC2 and turns the RXT
block into a v2 trailer, so APRS-IS, digipeating, TNC, JSON and SD logging
work unchanged. `lora.txFormat` selects what it sends: 0 text (default), 1
native, 2 text then native. When sending native, the v2 trailer of the packet
is moved into the RXT block, and a packet that cannot be encoded goes out as
text.

RF test on 29 September 2026, F4MLV-2 and F4MLV-15 (`ttgo-lora32-v21_SD`):

- a status and a compressed position sent native by F4MLV-2 were decoded by
  F4MLV-15 and relayed with its RXT tuple, byte for byte;
- with both sending native, F4MLV-2 sent 34 bytes instead of 45 and decoded
  the native relay of F4MLV-15, RXT block included; the TTH measured by
  F4MLV-15 was 2,095–2,499 ms against about 2,725 ms when relaying text;
- F4MLV-2 uploaded the relayed frame to APRS-IS without trailer, a `{world}`
  comment intact, and no other iGate uploaded a copy.

## Open points

- Mic-E, messages, telemetry and objects travel as text. They are about 12 %
  of the airtime at F4MLV-10 and 17 % on the N7UV feed.
- Relay state is the TNC2 path, with its `*` markers. A native relay state
  (requested, used, remaining) is not defined yet.
- A native frame is relayed by converting it to text and back: the relay
  logic works on the TNC2 path.
- The tracker (T-Deck) does not handle native frames yet.
