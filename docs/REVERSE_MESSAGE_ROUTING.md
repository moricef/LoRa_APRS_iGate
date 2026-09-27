# APRS reverse message routing

The iGate learns a return path only from a packet it has actually received over
LoRa. When APRS-IS later supplies a message for that recently heard station,
the iGate wraps it in an APRS third-party frame and uses the learned relays in
reverse order.

For example, a packet received as:

```text
N7UV-4>APLRT1,N7UV-6,SOMTNP*,WIDE2-1:...
```

teaches this RF return path:

```text
SOMTNP,N7UV-6
```

Both `N7UV-6*,SOMTNP*` and the canonical `N7UV-6,SOMTNP*` form are accepted.
The used path is the complete portion ending at the last `*`; unconsumed
aliases and Internet-only components are not copied into the return route.

## Route selection policy

The first copy of a logical packet received within the 25-second duplicate
window wins. Later copies carrying the same source and information field do not
replace its route, even if they arrived through different digipeaters. This is
a deliberate preference for the route that delivered the packet first, not a
claim that it is always the strongest or most robust RF path.

A directly received first copy therefore records `DIRECT`, even if a relayed
copy follows. A later packet with different information can teach a new route.

## Safety and lifetime

- A learned route expires with the existing `rememberStationTime` entry.
- After reboot, a station must be heard again before APRS-IS messages are sent
  to it over RF.
- There is no fallback to `Config.beacon.path`. If no valid route is known, the
  message is not transmitted.
- The mechanism applies only to APRS-IS messages for recently heard stations;
  it does not alter beacon paths or generic WIDE routing.
- Fill-in, regional and explicit-path digipeater modes all accept their own
  callsign as the next unused element of a learned source route.

Serial diagnostics use the `[RETURN-PATH]` prefix for learned, selected and
rejected routes.

## RF validation — 2026-09-27

Test firmware was installed on F4MLV-15 (iGate) and F4MLV-2 (digipeater).
F4MLV-15 used `messagesToRF=true` and filter `m/10 g/F4MLV-MC`.
Saving configuration rebooted the iGate, so the route was learned again after
the final configuration change.

The iGate transmitted a test RF packet under the simulated identity F4MLV-MC,
then received its actual RF repetition from F4MLV-2:

```text
F4MLV-MC>APLRG1,F4MLV-2*:>Reverse route relearn 20260927-8
[RETURN-PATH] Learned F4MLV-MC via F4MLV-2
```

A separate, authenticated APRS-IS test connection under F4MLV-14 sent:

```text
F4MLV-14>APLRG1,TCPIP*::F4MLV-MC :Test retour complet 20260927-9
```

The iGate's serial capture confirmed reception from APRS-IS, selection of the
learned route, and transmission:

```text
Rx Message (APRS-IS): F4MLV-14>APLRG1,TCPIP*,qAC,T2SYDNEY::F4MLV-MC :Test retour complet 20260927-9
[RETURN-PATH] Message to F4MLV-MC via F4MLV-2
---> LoRa Packet Tx : F4MLV-15>APLRG1,F4MLV-2:}F4MLV-14>APLRG1,TCPIP,F4MLV-15*::F4MLV-MC :Test retour complet 20260927-9
```

The RF capture then received the actual repetition:

```text
F4MLV-15>APLRG1,F4MLV-2*:}F4MLV-14>APLRG1,TCPIP,F4MLV-15*::F4MLV-MC :Test retour complet 20260927-9
LOCAL -- RSSI:-56 SNR:+9.00 FO:-2081
```

This validates RF learning, APRS-IS reception, transmission on the learned
explicit path, and RF repetition by the requested digipeater. F4MLV-MC was a
simulated source, not a physical receiver. At this stage, delivery and ACK at
an actual end station and multiple-hop return routes on real hardware had not
yet been tested; the follow-up tests below cover both cases.

Graywolf message #104 (`065`) also proved APRS-IS reception and iGate
transmission, but its sender was F4MLV-2. That same digipeater rejects its own
source callsign in the inner third-party packet, so it could not validate the
second hop. A TNC injection subsequently proved third-party repetition alone;
only the F4MLV-14 APRS-IS test above exercised both stages in one test.

Use distinct numeric SSIDs in the range 0–15 for the APRS-IS test sender and
connection. Do not reuse the active iGate login for a second connection.
The APRS-IS sender must also differ from each relay on the learned route.

### Physical receiver and ACK: F4MLV-7

After acquiring its GNSS fix, F4MLV-7 transmitted a manual beacon. F4MLV-15
received the direct copy first and logged:

```text
[RETURN-PATH] Learned F4MLV-7 via DIRECT
F4MLV-7>TRUWV3,WIDE1-1,WIDE2-1:`w25l#X[/"=@}
```

The later copy through F4MLV-2 did not replace the direct route. The APRS-IS
test connection sent a numbered message and the iGate selected `DIRECT`:

```text
F4MLV-14>APLRG1,TCPIP*::F4MLV-7  :Test retour avec ACK{R001
[RETURN-PATH] Message to F4MLV-7 via DIRECT
---> LoRa Packet Tx : F4MLV-15>APLRG1:}F4MLV-14>APLRG1,TCPIP,F4MLV-15*::F4MLV-7  :Test retour avec ACK{R001
```

The operator confirmed receipt on the physical tracker from F4MLV-14 and its
ACK. F4MLV-15 received the ACK directly over RF (`RSSI:-72 SNR:+11.25`) and
uploaded it. The separate APRS-IS connection received:

```text
F4MLV-7>APLRT1,WIDE1-1,WIDE2-1,qAR,F4MLV-15::F4MLV-14 :ackR001
```

This proves reception and ACK at an actual station, with the ACK reaching
APRS-IS through F4MLV-15. A concurrent copy of the downlink from F4JQT-10 via
F4MLV-10 was also captured, so this test does not uniquely identify which RF
downlink the tracker acknowledged. Its learned route was direct; physical
receiver/ACK validation specifically through a learned relay required an
isolated test.

### Isolated physical delivery through one relay

The radio profiles were deliberately separated so that the downlink could not
reach F4MLV-7 directly:

- F4MLV-15 transmitted on the EU profile (433.775 MHz, SF12/CR5) and received
  on the Poland profile (434.855 MHz, SF9/CR7).
- F4MLV-2 received on the EU profile and repeated on the Poland profile.
- F4MLV-7 used the Poland profile for both reception and transmission.

After a synthetic RF packet taught F4MLV-15 that F4MLV-7 was reachable through
F4MLV-2, F4MLV-14 sent numbered message `P201` through APRS-IS. F4MLV-2
received the EU transmission from the iGate and repeated it on the Poland
profile. F4MLV-7 received the message and generated the ACK; because the iGate
was listening on the Poland profile, it received that ACK directly and
uploaded it to APRS-IS:

```text
F4MLV-7>APLRT1,WIDE1-1,WIDE2-1,qAR,F4MLV-15::F4MLV-14 :ackP201
```

The incompatible profiles on F4MLV-15 and F4MLV-7 make a direct downlink
impossible in this setup. This therefore validates physical message delivery
through the learned one-relay route, ACK generation by the destination, and
return of that ACK to APRS-IS.

### Isolated two-relay path

A second isolated test exercised consumption of a two-element explicit path.
F4MLV-15 again transmitted on the EU profile and received on the Poland
profile; F4MLV-2 received on EU and transmitted on Poland. F4MLV-7 was
temporarily configured as a Poland-profile digipeater accepting its own
callsign as an explicit alias.

A synthetic RF source packet for F4MLV-MC carried the used path
`F4MLV-7,F4MLV-2*`. F4MLV-15 consequently learned the reverse route
`F4MLV-2,F4MLV-7`. F4MLV-14 then sent APRS-IS message `P303`. The final RF
capture after both relays had consumed their respective path elements was:

```text
F4MLV-15>APLRG1,F4MLV-2*,F4MLV-7*:}F4MLV-14>APLRG1,TCPIP,F4MLV-15*::F4MLV-MC :Test deux relais isoles{P303
```

The frequency and modulation split forced the first hop through F4MLV-2, while
the final capture with both explicit callsigns marked as used proves that
F4MLV-2 and then F4MLV-7 processed the route in the expected order. F4MLV-MC
was synthetic in this test, so `P303` validates two-relay route execution, not
delivery or ACK by a third physical endpoint. Physical endpoint delivery and
ACK had already been established separately by `P201`.

### Remaining network-level limitation

The tests also showed that APRS-IS does not elect one exclusive RF gateway for
a message. Multiple eligible iGates can receive the same downlink from APRS-IS
and independently transmit it on RF. APRS-IS duplicate handling must therefore
not be treated as coordination between transmitting iGates.

The learned reverse path solves the RF-path asymmetry for an individual iGate:
direct, one-relay and two-relay cases are now demonstrated. It does not solve
gateway coordination when several iGates cover the same destination. That is a
separate network-level problem and remains to be characterized or mitigated.

### Restoration after testing

All three devices were returned to their normal EU configuration after the
isolated tests. The following beacon confirmed F4MLV-7 transmitting on
433.775 MHz at 293 bit/s, repetition by F4MLV-2, and injection into APRS-IS by
F4MLV-15:

```text
F4MLV-7>TRUWV3,F4MLV-2*,WIDE2-1,qAR,F4MLV-15:`w25l"x[/"=C}LoRa APRS Tracker Batt=3.99V ( 79%) 433.775MHz 293bps
```
