# Full-image OTA transfer through a relay — RF test, 13 November 2026

[Version française](VALIDATION_OTA_IMAGE_COMPLETE_20261113_fr.md)

> Test report. The conclusions apply only to the setup, software and radio
> conditions described below.

## Objective

The purpose of this test was to transfer a complete firmware image by radio
from F4MLV-2 to F4MLV-15 through F4MLV-10 over an attenuated link. The test
also had to exercise recovery after a link loss, verify the image in the
inactive partition and return every participant to its normal radio profile.
The received image was not to be activated.

## Setup and parameters

- controller and USB bridge: F4MLV-2;
- radio relay: F4MLV-10;
- offline target: F4MLV-15;
- the attenuation cascade fitted between F4MLV-15 and its antenna on
  8 November was left in place;
- all three stations ran the same unified iGate OTA firmware:
  `4.0.2RXT-OTA`, build `2026-11-11 15:00:28 UTC`;
- full uncompressed image: 1,782,064 bytes, SHA-256
  `020e39263efa5d4fc0e5d06f88edc83b0c47928801a8155c5269292a137b0f86`;
- control profile: 434.100 MHz, BW 125 kHz, SF12, CR 4/5, 20 dBm;
- transfer profile: 434.500 MHz, BW 500 kHz, SF7, CR 4/5, 9 dBm;
- 160-byte blocks and 4,096-byte durable pages;
- activation explicitly disabled.

### Attenuation cascade

The cascade consists of two π attenuator sections connected in series. Each
section uses one 820 Ω series resistor and two 51 Ω shunt resistors, for a
total of two 820 Ω and four 51 Ω resistors.

Its insertion loss had been measured as **53 dB with a NanoVNA**. On the
radio test link, however, the observed difference with and without the
cascade was only **28 dB**. During that earlier measurement, MLV-15 at 2 dBm
was received by MLV-10 at **-113 dBm** with **+3.75 dB SNR**. The physical
assembly was unchanged for the full-image transfer, but these values were not
measured again on 13 November. The 53 dB NanoVNA result therefore describes
the measured attenuator itself, whereas 28 dB is the difference previously
observed on the radio setup; the two figures are not interchangeable.

The host client was `tools/lora_ota/send_integrated.py`. Recovery after the
profile lease expires is already part of commit `e559036`. The `OTABRIDGE`
startup and clean `BRIDGE OFF` shutdown used with the integrated iGate bridge
were still local, uncommitted client changes during this test.

## Results

The target first confirmed that it was offline. The participants then
negotiated the fast profile and started the transfer at offset zero.

The link was lost after 831,488 bytes had been committed to flash. The client
waited for the 60-second profile lease to expire, opened a new session with a
normal-profile `HELLO`, negotiated the fast profile again and presented the
manifest once more. The target resumed at exactly 831,488 bytes, and the
transfer then ran to completion.

| Measurement | Result |
| --- | ---: |
| Verified bytes | 1,782,064 |
| Committed pages | 436 |
| Repair cycles | 15 |
| Request retries | 9 |
| Link losses | 1 |
| Cumulative recovery time | 103.5 s |
| Reported transfer duration | 3,608.2 s |
| Average useful throughput, including recovery | 493.9 bytes/s |

The target accepted the final image verification. The client then confirmed
the return to the normal radio profile and recorded that no activation had
taken place. The complete image therefore remains in F4MLV-15's inactive
partition.

After the test, F4MLV-2 and F4MLV-10 were read back on their 433.775 MHz
service configuration. F4MLV-10 matched its pre-test backup, with digi mode 2
and RF beacons enabled. F4MLV-15 returned to its saved 434.100 MHz laboratory
profile. The attenuation cascade remains fitted.

## Conclusion and limits

This run validates a complete relayed transfer, preservation of durable
progress across a link loss, resumption through a new session and final image
verification on an offline target.

It does not validate activation, booting the received firmware or rollback to
the previous image. It is one complete run over one route and one attenuated
test setup. The client-side iGate bridge startup and shutdown changes still
need to be committed before they can be described as published; the recovery
logic itself is already committed.

The complete logs are retained locally under
`LoRa_APRS_iGate-ota/logs/ota-full-image-cascade-20261113-191807Z/`.
