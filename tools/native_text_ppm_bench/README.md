# Native format with PPM-coded free text: size benchmark

Host benchmark used for the measurements posted in
[moricef/LoRa_APRS_Tracker#7](https://github.com/moricef/LoRa_APRS_Tracker/issues/7).
It is not a radio format and is not part of the firmware build.

## What it measures

One TNC2 packet per line on stdin. For each packet it prints the frame size
in bytes, 3-byte LoRa prefix included, without any RXT block:

| Column | Frame |
|---|---|
| `plain` | current text frame |
| `native_huffman` | native draft 0 as in the firmware, free text coded with Huffman |
| `native_ppm_text` | native draft 0, free text coded with APRS-PPM |
| `native_best_text` | native draft 0, free text coded with the shorter of Huffman and APRS-PPM, chosen per packet |
| `native_raw_source` | `native_best_text` with the source as a raw text element (`0b11 lllll` + text, already defined by draft 0) instead of base 40; computed, not encoded |
| `ppm_whole` | standalone APRS-PPM frame of the whole packet |
| `clear_source_ppm_rest` | `SRC>` in clear followed by the APRS-PPM frame of the rest of the packet |

The free text is the part of the information field that draft 0 does not
structure. Its PPM coding is the PPM frame of that text without the 4-byte
PPM header (prefix, version, length); the native frame already stores the
text length.

Every native and PPM frame is decoded and compared with the input. Packets
the PPM encoder declines are counted at their plain size. On stderr the
program prints mean sizes, byte savings and computed time-on-air savings
(BW125, 8-symbol preamble, explicit header, CRC, low data rate optimisation
at SF11 and SF12; computed, not measured on air).

## Limits

The coder chosen for `native_best_text` is **not written into the frame**:
the decoder reads it from a variable set by the encoder (`lastTextPpm`). The
sizes therefore leave that signal out. It could fit in the two unused values
of the draft 0 "information kind" field, but this is not implemented.

The PPM coding of the free text uses the v1 model trained on whole packets,
not retrained on free text alone.

## Versions used for the posted figures

- Native encoder: `src/native_aprs.cpp` and `src/rxt_protocol.cpp` of
  moricef/LoRa_APRS_iGate commit `2cebfe6`, plus the changes of this branch,
  compiled only with `-DNATIVE_TEXT_PPM_BENCH`.
- APRS-PPM: `lib/AprsCompress/src` of radiohound/LoRa_APRS_iGate, branch
  `feature/order3-compression`, commit `f82431a` (model v1). Not run with
  APRS-PPM 2.0.0.

## Build and run

    make AC=/path/to/LoRa_APRS_iGate/lib/AprsCompress/src
    make AC=... check            # synthetic vectors only
    ./bench < packets.txt > sizes.tsv

`vectors.txt` holds a few synthetic packets so that the program can be run
without captures. The Ariège and Arizona captures used for the posted
figures are not published.

Results with the versions above:

| Capture | packets | native_huffman | native_best_text | native_raw_source | ppm_whole | clear_source_ppm_rest |
|---|---:|---:|---:|---:|---:|---:|
| Ariège | 26,303 | 32.8 % | 41.5 % | 36.8 % | 47.7 % | 42.0 % |
| Arizona | 990 | 25.0 % | 46.0 % | 44.0 % | 50.0 % | 47.7 % |

Byte savings against `plain`.
