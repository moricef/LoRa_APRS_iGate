# Authenticated remote control over LoRa APRS — design draft

Status: v1 implementation in progress, **disabled until a key is installed**. This is a
separate application protocol carried by LoRa APRS messages; it does not change
the LoRa APRS JSON reception protocol or RXT.

## Validated v1 scope — KISS principle

The first version is deliberately minimal: one random secret key per iGate,
one Graywolf controller, one persistent counter, a readable clear-text command
and an HMAC computed by standard cryptographic libraries. It protects only
`?EM=ON/OFF`, `?TX=ON/OFF` and `?COMMIT`. Once enabled, unsigned forms of
these state-changing commands are rejected.

Version 1 has no multiple accounts, certificates, clock synchronization or
shared key between several controllers. Advanced key rotation and multiple
controllers remain outside this scope.

## Existing behavior and scope

`src/query_utils.cpp` accepts privileged `?EM=ON/OFF`, `?TX=ON/OFF` and
`?COMMIT` commands when the apparent source matches the configured manager
list. `src/station_utils.cpp` also permits a trailing `*` wildcard in that
list. The RF-only switch limits the transport, not source spoofing. Current
APRS acknowledgements are transport receipts, not proof that a command was
authenticated or applied.

The first protected scope should be exactly those state-changing commands.
Public read-only queries (`?APRSV`, `?APRSP`, `?APRSL`, `?APRSSR`) need not change.
No firmware implementation should be enabled until sender and receiver test
vectors, persistence behavior and recovery have been agreed.

## Security properties required

1. A receiver acts only on a command intended for its **exact** configured
   control identity. A copied command for another asset must fail.
2. The controller identity, target identity, protocol version, key identifier,
   counter and **exact command bytes** are authenticated together. The mutable
   TNC2 path, digipeater markers and APRS-IS q constructs are not signed.
3. A recorded packet cannot execute a command again, including after a reboot
   or when identical copies arrive through multiple RF paths.
4. An incorrect tag, unknown key, old counter, malformed message or unsupported
   action cannot change configuration. Authentication occurs before dispatch
   to existing query handlers.
5. The receiver's persistent anti-replay state is updated *before* the action.
   A power loss may consume a counter without performing the action; it must
   never enable the same counter to execute the action twice. The controller
   can query state and submit a fresh counter.
6. A transport APRS ACK and an authenticated application result are distinct.
   The sender must not label a command successful merely because an APRS ACK
   arrived or because Graywolf queued a TNC2 packet.

## Proposed mechanism

Use a secret specific to the iGate and its single Graywolf controller, plus a
monotonically increasing counter.
Compute a message authentication code over a byte-exact, versioned envelope
containing the fields in rule 2. HMAC-SHA-256 is the initial candidate; the
tag length and its printable on-air encoding must be fixed after an airtime and
security review. This is **not** a bare six-digit HOTP code appended to a
command: such a code is not bound to the requested action. The counter has the
useful no-clock property of HOTP, but the complete command is authenticated.

### v1 wire format

The APRS message body is:

```text
!RC1:A:<uppercase-base36-counter>:<command>:<base64url-tag>
```

`A` is the v1 key identifier. The counter is a non-zero 64-bit integer in
uppercase base 36 without leading zeroes. The only accepted commands are
`EM=ON`, `EM=OFF`, `TX=ON`, `TX=OFF` and `COMMIT`. The tag is the first 12
bytes of HMAC-SHA-256 encoded as unpadded Base64URL, exactly 16 characters.

The authenticated bytes are concatenated in this order:

```text
"LORA-APRS-RC"
0x01
uint16_be(controller length) || exact controller bytes
uint16_be(target length)     || exact target bytes
"A"
uint64_be(counter)
uint16_be(command length)    || exact command bytes
```

Strings are their transmitted ASCII bytes without a terminator. The APRS path,
`*` marks, q constructs and APRS message number `{nnn` are excluded because
they may change in transit.

v1 test vector: Base64URL key
`AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8`, controller `F4MLV-2`, target
`F4MLV-10`, counter 71 (`1Z`), command `TX=OFF`, tag
`OLkaJxyKIXBM9SrF`. The complete body is:

```text
!RC1:A:1Z:TX=OFF:OLkaJxyKIXBM9SrF
```

Keep one secret and one high-water counter on each asset.
Counters are unsigned and must never wrap or reset with a reboot. The sender
must reserve/persist a counter before emitting a command, so a crash cannot
reuse it. The receiver must durably advance its high-water counter before
dispatch. Do not allow a wide look-ahead window or share the key with another
operator application.

The device may answer a duplicate with an explicit "already accepted" result,
but it must never re-execute it. An application result should identify the
authenticated request, state whether it was applied, rejected or already
accepted, and be authenticated if the controller is to rely on it. Reply
authentication and persistence of the last result require a separate design
decision; neither is supplied by the ordinary APRS ACK.

## Deployment and compatibility

- Provision secrets through a trusted local/admin path, not in a plaintext RF
  command. Never expose them in JSON configuration backups, logs, WebUI GETs,
  APRS-IS, RXT or packet diagnostics. Define key replacement and loss recovery.
- Keep protected commands disabled by default until a key is installed.
  Decide explicitly whether legacy callsign-only state-changing commands are
  then rejected; silently accepting both would nullify the protection.
- Preserve receive-only JSON ingestion. A JSON event, even one whose packet
  is AX.25-representable, never authorizes remote-control execution.
- Graywolf can be the first controller, using its authenticated message
  composer and explicitly configured TNC2 TX channel. The wire format must
  remain application-independent so another client can generate commands.
- Existing APRS message addressee fields have a nine-character limit. Either
  constrain the first version's control target identity to that limit or
  define a separate TNC2-compatible control datagram; do not pretend all
  extended identities fit the historic APRS message field.
- Check the applicable amateur-radio rules for authenticated control traffic
  before field deployment. This draft does not assert that every jurisdiction
  permits every proposed on-air encoding.

## Tests required before RF enablement

Independent sender/receiver vectors; modified action/target/source/counter;
wrong key and key ID; duplicate reception by two iGates; out-of-order delivery;
sender and receiver reboot at every persistence boundary; malformed and
oversize fields; no action on APRS ACK alone; explicit result states; exact
extended identity comparison; maximum-size/airtime checks on each supported
radio profile. A hardware test must confirm the same command is never applied
twice after retransmission or power loss.

## Decisions left beyond v1

1. How authenticated results are returned, deduplicated and displayed.
2. Advanced key rotation, plus physical recovery for inaccessible
   remote assets.
3. Migration policy for existing manager-whitelist commands and the RF-only
   switch.

References: [RFC 4226 (HOTP)](https://www.rfc-editor.org/rfc/rfc4226) and
[RFC 2104 (HMAC)](https://www.rfc-editor.org/rfc/rfc2104).
