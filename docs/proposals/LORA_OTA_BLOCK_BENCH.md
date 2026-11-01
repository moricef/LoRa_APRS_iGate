# Evaluating LoRa OTA block sizes

[Français](LORA_OTA_BLOCK_BENCH_fr.md)

## Question and criteria

Compare the cost of delivering and verifying the same bytes, first directly
and then through a relay. The primary criterion is total airtime; elapsed
time and transfer success remain separate criteria. No optimal size is
claimed before measurements.

This protocol and its benchmark are implemented on the OTA branch.
A [hardware test on 1 November](../tests/2026-11-01/OTA_BLOCK_SIZES.md)
validates five sizes directly, then 128- and 175-byte blocks through one
relay, using the same 8 KiB sample and SHA-256 verification in flash.
Airtimes were recalculated after identifying stale LDRO state in RadioLib's
calculation; the report distinguishes these values from raw counters.
These tests do not replace the comparative matrix below.
Previous full-image transfers and hardware recovery tests used 128-byte
blocks.

## Fixed and compared variables

Within a series, keep the same source file, 65,536-byte prefix, SHA-256,
boards, route, radio settings, antennas and antenna orientation. Flash
pages remain 4,096 bytes: their size and write frequency do not vary with
the block size.

Start with **32, 64, 96, 128, 160 and 175 data bytes**. Direct tests may
also include 192 and 204 bytes. The authenticated header costs 51 bytes;
the relay envelope adds 29 bytes. The maximum radio payload is 255 bytes,
giving limits of 204 data bytes directly and 175 through a relay.
The last blocks of a page or file may be shorter.

The bitmap adapts to the number of blocks per page: 4, 8, 12 or 16 bytes.
Its overhead is included in the result. At 128 bytes, the legacy messages
and four-byte bitmap remain unchanged.

Retransmissions use the same algorithm for every size. The relay retains
its page cache and three local repair rounds. Response timeouts are
calculated with the same formulas from the profile and block count; they
are not adjusted manually to favour a size. On a direct link, the request
timeout does not depend on the length of a relay round that does not exist.

## Test matrix

1. Direct link with a comfortable signal level and a documented environment.
2. Attenuated direct link using a documented obstacle, distance or attenuator;
   keep this setup for every size.
3. Link with documented competing traffic, if that traffic can be kept
   comparable between runs.
4. Confirmation of the selected sizes with the intermediate relay.

Start with five repetitions of every size for each condition and profile.
Shuffle the order within each repetition and retain the seed. A different
condition or profile forms a separate series. Increase the repetition
count if variability prevents distinguishing the sizes. A P95 from five
runs is only a descriptive statistic, not a precise estimate of rare cases.

The operator establishes and describes the physical conditions. The
benchmark does not treat deliberate software loss as an attenuated RF
link. The earlier fault injections are not used to rank block sizes.

With a relay, the target must wait while the controller fills the cache.
A round exceeding the maximum 60-second lease is rejected before the test.
Some slow profiles, particularly SF12, can therefore be compared directly
but require scheduling changes before evaluation through the relay.
The benchmark does not shorten pages to bypass this limit and distort
the comparison.

## What a run does

The controller arms an authenticated session and checks firmware
capabilities, the absence of pending boot confirmation, and Wi-Fi settings.
A refusal at this stage does not remove the existing recovery checkpoint;
the session expires through its inactivity timeout. After these checks,
the controller explicitly clears the previous laboratory checkpoint and
opens a new session.
It negotiates the temporary profile and checks that target Wi-Fi and the
access point are disabled. The relay retains its diagnostic access, if used.

Timing starts before WINDOW_START and ends after verification of the prefix
written to the inactive partition. The sample is not treated as a valid
firmware image: it is read back from flash and its SHA-256 is compared with
the manifest. **No application activation or boot confirmation is requested.**

After the run, the controller retrieves the counters, returns to the normal
profile and closes the session with ABORT. The next run starts from zero,
without using previously written pages. This cleanup and initial negotiation
are outside the compared interval; their traces are retained.

Prefix verification is restricted to laboratory firmware built with
RFOTA_DIAGNOSTICS. It never places the image in READY state and does not
permit activation.

## Counters and results

The controller counts transmissions confirmed by TXDONE. The target and
relay count successful transmissions, bytes and airtime calculated by
RadioLib for each actual frame. This includes replies lost at the
controller and local relay repairs. Counter queries and their replies are
excluded from the measurement counters. Driver errors are reported and
invalidate a run rather than being counted as successful transmissions.

This **airtime is calculated**, not measured with an RF analyser. It uses
the transmissions actually confirmed by the radios, complete on-air frame
sizes and active profiles; it is not simply useful bytes divided by the
nominal bit rate.

Duration is measured with a monotonic clock. Boot identifiers allow the
benchmark to reject a run in which the target or relay restarted. A nonce
change is not reported as a successful transfer.

Retain for each run: block size, profile and condition, repetition and
order, byte count and SHA-256, success or error, total duration, aggregate
airtime across all transmitters, controller/target/relay transmission counts,
controller retransmissions and local repairs. Results also record the ELF
fingerprints reported by the target and relay to identify the running
firmware. Keep bridge and device binaries with their SHA-256 hashes for
the series. JSONL traces contain requests and replies without exporting keys.

A failure stops the series; the failed run remains in trials.json and
trials.csv. It is not hidden in an average. The summary distinguishes
attempted and successful runs and gives the mean, median, P95 and maximum
for successful runs. An incomplete series does not select a winner.

## Prepare without accessing the boards

From the OTA worktree:

```bash
python3 tools/lora_ota/block_bench.py plan \
  --radio-profiles tools/lora_ota/radio-profiles.example.json \
  --blocks 32,64,96,128,160,175 --repeats 5 --seed 20261101 \
  --condition 'direct bench; antennas and distances to be documented' \
  --output logs/block-bench-20261101/plan-direct.json
```

This mode reads no secrets, opens no ports and contacts no equipment.
The displayed airtimes are theoretical minima per page, without losses,
negotiation or processing delays; they are not RF test results.

## Install and run the benchmark

Targets and relays must run the new compatible firmware. Back up their
configurations, keys and complete flash contents before installation.
Choose the setup and profiles before flashing. The inventory distinguishes
radio identities from management access; MLV-10 is at the end of its mast,
not a board whose USB connection can be assumed available.

Keep the laboratory firmware installed throughout the campaign. Temporary
management access loss pauses the work; it does not trigger restoration or
another flash. Board restoration is a separate operation, after the tests
or when requested.

```bash
pio run -e ttgo-lora32-v21-ota-a -e ttgo-lora32-v21-ota-relay
pio run -d tools/lora_ota -e bridge_fast
```

After laboratory provisioning of the bridge and target:

```bash
python3 tools/lora_ota/block_bench.py run \
  --bridge /dev/serial/by-id/BRIDGE_PORT \
  --key-file PRIVATE_PATH/secret.base64 \
  --counter-file PRIVATE_PATH/counter \
  --image IMAGE_PATH/firmware.bin --sample-bytes 65536 \
  --radio-profiles tools/lora_ota/radio-profiles.example.json \
  --blocks 32,64,96,128,160,175 --repeats 5 --seed 20261101 \
  --condition 'precise description of the direct setup' \
  --output logs/block-bench-20261101/direct-comfortable-signal
```

For a relay, add `--via F4MLV-10`,
`--route-key-file PRIVATE_PATH/route.base64` and
`--relay-status-url MANAGEMENT_URL/lab/ota/route`. The last parameter is an
actually accessible management address, possibly through an SSH tunnel;
it is not another block transport. The relay must announce window_geometry=2
and provide its counters, otherwise the benchmark refuses the series.
Missing support is not treated as a series of RF losses.

Results, captures, keys, backups and counter files remain in local directories
ignored by Git. Each series requires a new directory: the benchmark refuses
to overwrite an existing series.

## Protocol extension

- Legacy WINDOW_START: 40-byte manifest, 128-byte blocks.
- Extended WINDOW_START: the same manifest followed by a two-byte big-endian
  block size, from 32 to 204; maximum 175 through a relay.
- WINDOW_QUERY and its reply bitmap use the width calculated for a complete
  page. A bitmap is a big-endian integer whose bit zero represents the first
  block. Bits beyond the final page remain zero.
- Block alignment restarts at each 4 KiB page, including when the size does
  not divide 4,096. A block never crosses a page boundary.
- Durable recovery remains a page offset independent of block size. A new
  session may use a different size at the same checkpoint; an active session
  refuses a change in block geometry.
- Opcode 25: authenticated query of TX counters and a boot identifier.
  Opcode 26: SHA-256 verification of a written sample, available only on
  diagnostic targets; no activation.

Legacy 128-byte exchanges remain compatible. Extended exchanges require
updated equipment along the entire route. The controller also checks the
bitmap width reported by the target.

## Decision after measurements

Retain the sizes that minimise airtime with acceptable reliability, then
compare their durations and variability. Confirm these choices with
full-image transfers along the intended deployment route. The
send_integrated.py client accepts `--windowed --block-size N` for this step;
full verification and activation remain those of the normal OTA protocol.

A result from two nearby boards does not by itself select the size for a
distant relay. This benchmark does not implement automatic block-size
adaptation during a transfer.

## Reference

[Malumbres et al., Sensors 2024, 24(7), 2104](https://pmc.ncbi.nlm.nih.gov/articles/PMC11014374/)
studies, among other factors, how fragment size and repairs affect LoRa
distribution time. Its results for multiple recipients at a fixed bit rate
do not determine the optimum for our relayed route.
