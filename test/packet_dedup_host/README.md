# Packet destination de-duplication host test

This harness compiles the production packet fingerprint and destination cache
on the host. It verifies independent return-route and digipeater claims, exact APRS
information bytes, RXT removal, the 25-second window, `millis()` wraparound and
the bounded cache.

Mic-E fingerprints also include the APRS destination: two latitudes encoded in
different destinations remain distinct for both digi and return-route claims,
while copies with added RXT tuples remain duplicates. Current, old and legacy
beta Mic-E identifiers are covered. Other packet types retain their existing
source/information identity.

It also exercises the local-message gate used by both RF consumers: a query
executes once in the 25-second window, the same reception produces at most
one ACK, and a later RF retry can be ACKed again. Sender, message number and
question changes remain distinct. Duplicate queries remain locally consumed
by the message handler instead of falling through to upload or relay.

Reply-ack cases verify exact ACK contents for `{MM}AA` and `{MM}`, reject
malformed numbers, and ensure that changing the piggybacked `AA` does not
execute the same query again. The normal ACK echoes the complete suffix as
specified by steps 3 and 4 of the
[APRS reply-ack mechanism](https://www.aprs.org/aprs11/replyacks.txt).

Run it with:

```sh
make test
```
