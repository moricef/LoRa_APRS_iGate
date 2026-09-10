# RXT host tests

This harness compiles the production `src/rxt_protocol.cpp` on the host. It
checks trailer handling and path parsing, including paths where every
digipeater keeps its own `*` marker.

See [the RXT telemetry documentation](../../docs/RXT.md) for the wire format,
configuration, output behavior and known limitation.

Run it with:

```sh
make -C test/rxt_host test
```

The RXT wire format itself remains compatible with N7UV's implementation.
Consequently, a legitimate APRS payload ending in 4, 8, or 12 printable
characters enclosed in braces cannot be distinguished from an RXT trailer.
