# Weather-symbol telemetry regression

Run `make -C test/telemetry_report_host test` from the repository root.
The test checks format selection, a real F1ZDB-13 sample, five-channel
reports, values above 255, and invalid base91 input.

When the position symbol is `_`, or the built-in weather sensor is enabled,
the battery/activity telemetry is emitted as a separate `T#` report at the
beacon interval. It follows the same RF/APRS-IS selection and RF beacon
frequency as the position. Other symbols keep base91 comment telemetry.
Sequence numbers wrap at 1000 for `T#`, and at 8281 for base91 reports;
the existing persistent sequence reservation is retained.

The report uses extended decimal telemetry supported by aprs.fi: it retains
raw values above 255 and omits unused trailing channels/digital bits.
PARM/UNIT/EQNS retain the same channel ordering and scaling as base91.
The external I2C voltage coefficient is 0.01, matching its encoder; the
external ADC voltage coefficient is 0.02.

The emitted test vectors were also decoded with Ham::APRS::FAP, checking
all channel values. This validates packet decoding, not live aprs.fi graph
updates or RF delivery. Firmware compilation was checked for
`ttgo-lora32-v21_SD`.
