# Validation RXT v2 — 29 septembre 2026

## Banc

F4MLV-15, F4MLV-2 et F4MLV-10 tournent sur `feature/rxt-v2`, variant
`ttgo-lora32-v21_SD` : build `2026-09-29 12:04:17 UTC` (`95a559d`) pour les
deux premières, build `2026-09-29 12:33:37 UTC` (`7ff658c`, avec `/sd/log`)
pour F4MLV-10, seule équipée d'une carte SD. F4MLV-2 injecte par son TNC et
décode ; F4MLV-15 et F4MLV-10 relaient en digi mode 2 avec RXT activé ; le
T-Deck (F4MLV-7) sert de relais non RXT en mode repeater. F4MLV-9 est une
identité synthétique. Captures : `logs/rxtv2-20260929/`.

Tests hôte : les cinq bancs passent, dont les vecteurs d'empreinte, le
contrôle du trailer au relais et avant APRS-IS, l'attribution dans l'ordre du
chemin avec empreintes identiques et la collision fortuite documentée.

## Chaîne à deux relais RXT — 12:17 UTC

Trame injectée : `F4MLV-9>APLRG1,WIDE1-1,F4MLV-15,F4MLV-10,RFONLY:>RXTV2-TEST`.

| Copie reçue par F4MLV-2 | Sauts décodés |
| --- | --- |
| après le T-Deck | `F4MLV-7<--F4MLV-9 NA` |
| après F4MLV-15 | `F4MLV-15<--F4MLV-7 RSSI:-76 SNR:+10.50`, puis la ligne NA |
| après F4MLV-10 | `F4MLV-10<--F4MLV-15 RSSI:-64 SNR:+11.50` et `F4MLV-15<--F4MLV-7 RSSI:-76 SNR:+10.50`, puis la ligne NA |

Les deux tuples sont attribués aux bons relais sans liste blanche, le relais
non RXT apparaît en NA, et la mesure de F4MLV-15 reste identique après le
second relais.

La même trame terminée par `{world}` donne les mêmes sauts, et la trame
décodée conserve `RXTV2-TEST2 {world}` intact.

Hors essai, une balise publique de `F4KOL-4` relayée par `F5ZQC-10` (non RXT)
puis par F4MLV-15 a été décodée avec `F5ZQC-10` en NA et le tuple de
F4MLV-15 correctement attribué.

## Passage vers APRS-IS — 12:29 UTC

F4MLV-15 en digi 2 avec APRS-IS coupé ; F4MLV-2, qui ne reçoit que des copies
relayées donc porteuses de tuples, est le seul iGate du banc à les envoyer.
Trames injectées sans `RFONLY`, chemin `F4MLV-15,F4MLV-10`.

| Reçu par APRS-IS | Constat |
| --- | --- |
| `…,F4MLV-15*,F4MLV-10,qAR,F4MLV-2:>RXTV2-IS-1229` | copie à tuple envoyée sans trailer |
| `…,qAR,F4MLV-2:>RXTV2-IS-1229B {world}` | tuple retiré, `{world}` intact |
| `…,qAR,F4BPJ-10:>RXTV2-IS-1229{\jf$?hcn/?}` | iGate tiers : trailer transmis tel quel |

Le journal série de F4MLV-2 montre deux envois par trame (copies de F4MLV-15
puis de F4MLV-10) ; la seconde, identique après retrait, a été dédupliquée
par APRS-IS.

## Journal SD de F4MLV-10

Lu à distance par `GET /sd/log?tail=65536` (65 536 octets en 1,1 s).

| Événement | Trailer |
| --- | --- |
| `PATH` sur la copie directe | ce n'était pas encore son tour |
| `RELAY` sur la copie de F4MLV-15 | tuple reçu `\jf$?`, empreinte `\` de F4MLV-15 |
| `RXT_TX` | tuple émis `hcn/?`, empreinte `h` de F4MLV-10 ; trame `…RXTV2-IS-1229{\jf$?hcn/?}` |
| `RXT_TX` pour `{world}` | trame `…RXTV2-IS-1229B {world}{\ju$@hgn/A}`, commentaire intact et trailer séparé |

## Limites

- Les iGates hors fork (ici F4BPJ-10) envoient les trailers RXT tels quels
  sur APRS-IS, en v1 comme en v2.
- Le réseau de Jon N7UV n'est pas testé ; ses digis sont encore en v1, non
  compatible avec v2.
- Le retrait par la seule forme, utilisé pour la clé de déduplication faute
  de chemin, n'est vérifié que par les tests hôte.
