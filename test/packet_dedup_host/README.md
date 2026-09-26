# Packet destination de-duplication host test

This harness compiles the production packet fingerprint and destination cache
on the host. It verifies independent APRS-IS and digipeater claims, exact APRS
information bytes, RXT removal, the 25-second window, `millis()` wraparound and
the bounded cache.

Run it with:

```sh
make test
```
