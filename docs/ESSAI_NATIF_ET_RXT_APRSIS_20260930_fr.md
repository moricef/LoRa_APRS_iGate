# Essai natif du T-Deck et trailers RXT sur APRS-IS — 30 septembre 2026

## Essai des trames natives en service réel

Le T-Deck F4MLV-7 est resté posé sur un bureau, avec une balise toutes les
5 minutes (Smart Beacon désactivé) et le mode digi désactivé. Il a émis en
natif de 07:05:41 à 09:02:34 UTC, puis en texte de 09:02:34 à 10:07:46 UTC.
Toutes les autres stations émettaient en texte et savaient recevoir le natif.

Les réceptions ont été relevées dans les flux TNC de F4MLV-2 et F4MLV-15,
enregistrés sur un PC, et dans le journal SD de F4MLV-10. Ce journal ne donne
qu'un temps depuis le démarrage ; ses heures ont été reconstituées, puis chaque
réception a été rattachée à la balise la plus proche vue par F4MLV-2 ou
F4MLV-15, l'horloge de F4MLV-10 dérivant de 25 à 46 secondes sur la matinée.
F1ZDB-10, trop éloigné, n'a reçu aucune balise en direct. Les 8 balises que le
T-Deck a aussi envoyées directement sur APRS-IS avant 09:03:44 sont exclues.

| Phase | Balises vues | F4MLV-2 | F4MLV-15 | F4MLV-10 |
| --- | --- | --- | --- | --- |
| Natif (117 min) | 22 | 22, −79 dBm, SNR 11,6 dB | 18, −64 dBm, SNR 11,6 dB | 16, −88 dBm, SNR 11,4 dB |
| Texte (65 min) | 11 | 11, −80 dBm, SNR 11,4 dB | 10, −64 dBm, SNR 11,0 dB | 9, −101 dBm, SNR 7,5 dB |

Le natif a été reçu aussi bien que le texte. Les écarts entre les deux phases
portent sur une ou deux balises par station, ce que des échantillons de cette
taille ne permettent pas d'interpréter. La baisse du signal chez F4MLV-10 est
une dérive continue sur toute la matinée, de −85 dBm vers 07:30 à −105 dBm vers
09:30, sans cassure au changement de format ; F4MLV-2 et F4MLV-15 recevaient au
même moment un niveau stable. F4MLV-10 a relayé les balises natives, et
F4MLV-15 les a envoyées sur APRS-IS sans modification.

Une balise Mic-E du T-Deck fait 29 octets en natif et 47 en texte ; à SF12,
125 kHz et CR 4/5, elle dure environ 1,6 s au lieu de 2,3 s d'après la formule
de temps d'antenne. L'essai ne dit rien de la portée en limite de couverture :
toutes les liaisons avaient une marge confortable.

F4MLV-10 était en mode éco pendant presque tout l'essai, activé puis désactivé
par des commandes distantes vers 07:12 et 10:05 UTC ; les deux phases sont
concernées de la même façon.

## Trailers RXT envoyés sur APRS-IS par des iGates tiers

Une écoute APRS-IS en lecture seule, avec le filtre
`d/F4MLV-10/F1ZDB-10/F4MLV-15/F4MLV-2`, a relevé de 09:44:29 à 10:29 UTC les
paquets relayés par nos digis qui sont arrivés sur APRS-IS.

Jusqu'à 10:03:54, 29 paquets relayés par F4MLV-10 (15) ou F1ZDB-10 (14) sont
arrivés sur APRS-IS. 28 portaient encore leur trailer RXT, et tous les 28 ont
été envoyés par des iGates qui ne tournent pas sous ce firmware : F4JQT-10 (8),
F4KOL-4 (5), F1IXL-10 (5), F4BPJ-10 (5), F5SPA-10, F4GCF-1, F4LBZ-10, F4INI-10
et F4GCF-10 (1 chacun). Le 29e, relayé par F1ZDB-10 puis par le digi tiers
F4DMQ-2, se termine par un caractère altéré à la place du trailer.

Après la désactivation de RXT sur nos digis dans la matinée, un seul relais est
apparu sur APRS-IS jusqu'à 10:29, à 10:07:15, envoyé par F4MLV-2 et sans
trailer. Un relais identique à la trame d'origine est éliminé comme doublon par
APRS-IS, ce qui est cohérent avec cette quasi-absence.

Sur ce réseau, où la plupart des iGates ne retirent pas le trailer, RXT dans le
champ de données atteint donc APRS-IS pour presque chaque relais entendu par un
iGate tiers. Ces copies ne sont plus identiques aux autres : elles apparaissent
en double sur aprs.fi et, pour une balise contenant de la télémétrie comme
celle de F6DEV-10, elles déclenchent l'erreur « Duplicate telemetry sequence ».

RXT a donc été désactivé sur nos digis dans la matinée.

## RXT dans un bloc caché

Les mesures ont ensuite été déplacées hors du texte : sur l'air, elles suivent
le paquet après un octet nul et un octet de marquage. Le firmware de Ricardo
lit la trame reçue comme une chaîne C et s'arrête à l'octet nul ; nos stations
lisent la trame octet par octet et retrouvent le bloc (`docs/RXT.md`).

Sur le banc, F4MLV-15 a été flashé avec le firmware upstream (`fc256de`) et
F4MLV-2 avec un build de test ajoutant un bloc factice contenant `>`, `:` et
`{`. La trame `NUL-T2` a été reçue par F4MLV-15 sans trace du bloc, envoyée
propre sur APRS-IS en une seule copie, et relayée sans le bloc par F4MLV-15 et
par le digi tiers F5ZQC-10.

Avec le firmware définitif sur F4MLV-15, F4MLV-2 a retrouvé la mesure de
F4MLV-15 dans le bloc caché, et une balise native du T-Deck relayée par
F4MLV-15 a donné le saut `F4MLV-15<--F4MLV-7` avec ses mesures. Une fois
F4MLV-10 et F1ZDB-10 passés au même firmware avec RXT réactivé, un seul paquet
passé par nos digis est apparu sur APRS-IS entre 14:17 et 14:57 UTC, propre ;
Graywolf affichait au même moment 40 liaisons, dont les mesures transmises
dans le bloc.

Le bloc caché ne survit pas à un digi tiers, qui ne relaie que le texte. Dans
le journal SD de F4MLV-10, 40 % des réceptions portant des mesures étaient
passées par un de nos digis puis par un digi tiers : ces mesures sont perdues.
Les trames à plusieurs mesures, déjà rares (4 % des relais de F4MLV-10),
deviennent exceptionnelles.

## Plage du SNR

Le codage RXT d'origine borne le SNR entre −9 et +12 dB. Sur 26 447 réceptions
de F4MLV-10, 18,7 % étaient sous −9 dB et 27,7 % des mesures transmises étaient
collées à −9 dB. La plage est passée à −24..+20 dB par pas de 0,5 dB, qui ne
laisse hors plage que 0,1 % des réceptions. Toutes les stations doivent
utiliser le même codage ; nos quatre stations ont été mises à jour ensemble le
30 septembre (build `2026-09-30 15:05:06 UTC`).

## Suite

RXT est réactivé sur F4MLV-10 et F1ZDB-10, dans le bloc caché. L'écoute
APRS-IS sur `firmin` continue pour confirmer l'absence de doublons sur une
journée avant publication sur le flasher. Le bloc caché, la nouvelle plage du
SNR et la perte des mesures après un digi tiers sont à présenter à Jon N7UV.

## Données

`logs/native-20260929/` : `pilot-tdeck.jsonl` (flux TNC et APRS-IS),
`F4MLV-10_sd_pilot.csv`, `F1ZDB-10_tnc_2026-09-30.log` et
`aprsis-relays-2026-09-30.log`. Ces fichiers restent locaux.
