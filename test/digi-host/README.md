# Digipeater host harness

This harness compiles the repository's actual `src/digi_utils.cpp` against
minimal host stubs. It exercises digipeater path handling without modifying
firmware source and without requiring a board or radio.

From the repository root:

```sh
make test-digi
```

The process exits non-zero if any case fails. On upstream `4.0.1`, the expected
result is `13 passed, 3 failed`; all failures are in the exhausted-hop cases
reported in issue #443. With PR #463 applied, the result is
`16 passed, 0 failed`.

To test a `digi_utils.cpp` from another worktree:

```sh
make -C test/digi-host DIGI_SRC=/path/to/src/digi_utils.cpp test
```

The `String` stub follows the relevant Arduino ESP32 `WString.cpp` semantics:
`replace()` replaces every occurrence, `remove()` removes one range, and
`indexOf()` returns `-1` when no match exists.
