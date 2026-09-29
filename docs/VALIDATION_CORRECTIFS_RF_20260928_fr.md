# Validation RF des correctifs — 28 septembre 2026

## Banc et preuves

Essais réalisés entre 20:35 et 20:42 UTC sur F4MLV-15 et F4MLV-2,
avec le T-Deck comme récepteur indépendant. Les deux TTGO exécutent le
variant `ttgo-lora32-v21_SD`, build `2026-09-28 19:34:05 UTC`, contenant les
cinq correctifs et la correction Mic-E intégrés dans `a85b3d6`.

SHA-256 du firmware :
`eeb8f1a698f7201c0f23e3edb2fc9dfcb8c3f21c2c396f7e47e01640d9be45d6`.

F4MLV-2 injecte les trames par son entrée TNC2 et les émet réellement en
LoRa. F4MLV-15 est l'équipement testé. Les réponses et répétitions sont
observées sur la liaison RF par F4MLV-2 et/ou le T-Deck, et corrélées avec
la capture série de F4MLV-15. F4MLV-9 est une identité synthétique de test,
pas un quatrième équipement. Les chemins déjà utilisés présents dans les
trames injectées sont synthétiques.

Les captures et scripts sont conservés localement dans
`logs/validation-20260928/`. Le script `analyse.py` vérifie **32 assertions**
sur ces captures, initialement toutes réussies, mais le contrôle N1 était
insuffisant : il acceptait une mauvaise source d'ACK. Ce résultat ne valide
donc pas le firmware avant déploiement. Ce dossier contient aussi des sauvegardes
de configuration avec des secrets : il ne doit pas être publié en bloc.

## Résultats

| Cas | Observation probante | Conclusion limitée à l'essai |
| --- | --- | --- |
| Requête répétée `R1` | Deux réceptions espacées d'environ 12 s, deux ACK, une seule réponse ; iGate et digi actifs sur F4MLV-15 | Un ACK par réception, sans double réponse dans la fenêtre de 25 s |
| Reply-ack `R2` | Réception de `{R2}A1`, puis `{R2}A2` ; émissions et réceptions RF de `ackR2}A1` et `ackR2}A2`, une seule réponse à la requête | Suffixe correctement recopié et changement de `AA` sans nouvelle exécution |
| Chemins locaux `P1/P2/P3` | ACK et réponses reçus avec `F4MLV-2,RFONLY`, puis `F4MLV-2,F4MLV-7,RFONLY`, puis `RFONLY` seul ; `beacon.path=WIDE1-1` | Chemin appris utilisé et inversé ; absence de relais en direct ; `RFONLY` après les relais |
| Apprentissage sans APRS-IS `N1` | Requête adressée au tactique `F4MLV-1` ; ACK émis par `F4MLV-15`, réponse émise par `F4MLV-1` ; les deux portent le chemin appris `F4MLV-2,RFONLY` | Chemin correct sans APRS-IS, mais **échec de l'identité de l'ACK** : il doit provenir de `F4MLV-1` |
| Casse | Paquet portant `f4mlv-15` reçu puis répété avec `f4mlv-15*` | Indicatif explicite reconnu en minuscules |
| Cross-freq | Identité tactique temporaire `F4MLV-1` ; `F4MLV-10` dans le chemin puis en source n'empêche pas la répétition ; `f4mlv-1*` est reçu mais non répété | Pas de confusion par sous-chaîne ; protection contre la boucle conservée |
| Mic-E | Destinations `490350` et `490351`, même source et même information : deux répétitions reçues ; nouvelle copie de `490350` après environ 18 s, par un autre chemin : décision DUP | Destinations distinguées ; changement de chemin seul toujours dédupliqué |
| Chemin retour Mic-E | Apprentissage successif via `F4MLV-2`, puis `F4MLV-7` pour les deux destinations ; la copie de la première par `F4MLV-8` ne remplace pas le chemin | La nouvelle position peut enseigner un chemin ; son doublon ne l'écrase pas |

Pour le cross-freq, F4MLV-15 reçoit en 433,775 MHz / SF12 / CR5 et émet en
434,855 MHz / SF9 / CR7. F4MLV-2 utilise les profils inverses. Les répétitions
capturées par F4MLV-2 ont donc réellement traversé les deux profils radio.

## Limite de taille

Les trames injectées utilisent le chemin explicite `F4MLV-15,RFONLY`.
Sa consommation ajoute un octet `*`. Les tailles ci-dessous sont celles de
la trame APRS texte, hors préfixe LoRa de trois octets.

| Taille reçue | Taille après marquage du relais | Résultat observé |
| --- | --- | --- |
| 245 | 246 | Ajout des six octets RXT ; trame de 252 octets reçue intégralement par le T-Deck |
| 246 | 247 | Répétition sans nouveau RXT ; 247 octets reçus intégralement |
| 251 | 252 | Répétition sans nouveau RXT ; 252 octets reçus intégralement |
| 252 | 253 | Diagnostic explicite de refus avant émission ; aucune émission correspondante dans la capture série |

## Limites de cette validation

- Les essais de chemin local valident les chemins des ACK et réponses
  effectivement émis et reçus sur RF. Ils ne démontrent pas une livraison
  isolée à travers deux relais physiques : les chemins entrants sont
  synthétiques et le T-Deck peut entendre directement F4MLV-15.
- Le T-Deck sert ici de récepteur de contrôle, pas d'émetteur d'ACK applicatifs
  pour les messages adressés à l'identité synthétique F4MLV-9.
- Le repli vers `beacon.path` sans route connue et le réveil ecoMode ne sont
  pas exercés sur matériel dans cette série.
- F4MLV-15 ne détecte aucune carte SD. L'absence d'une ligne `RXT_TX` dans un
  fichier SD lors du repli sans RXT n'est donc pas vérifiée sur matériel.
- Mic-E est exercé avec des trames synthétiques pour tester la déduplication,
  pas pour valider le décodage géographique d'un tracker Mic-E.
- L'ambiguïté entre suffixe RXT et commentaire légitime reste hors de ces
  corrections et de cette validation ; elle attend la discussion avec Jon.

## Série complémentaire — 28 septembre 2026, 22:12–23:51 UTC

### Banc

F4MLV-15 et F4MLV-2 exécutent le variant `ttgo-lora32-v21_SD`, build
`2026-09-28 21:49:19 UTC`, commit `36ac992`, qui corrige l'identité de l'ACK
révélée par N1. SHA-256 :
`32e7c112bde9537ba1d30c7e516d1c8e52d71feb8b815a948b0466245a0db18c`.
Les tests hôte passent (digi-host 64/64, avec deux cas d'identité du
répondeur).

F4MLV-10, digipeater en service sur la même fréquence, a l'émission coupée
pendant la série. F4MLV-2 injecte par TNC2 et reçoit en RF ; le T-Deck sert de
récepteur de contrôle. F4MLV-5, F4MLV-6 et F4MLV-9 sont des identités
synthétiques. Captures : `logs/validation-complementaire-20260928/`.

### Résultats

| Cas | Observation probante | Conclusion limitée à l'essai |
| --- | --- | --- |
| Identité `I4` | Tactique `F4MLV-1` configuré ; requête à `F4MLV-1` ; ACK et réponse émis par `F4MLV-1` | L'échec d'identité de N1 est corrigé |
| Identité `I3` | Même configuration ; requête à `F4MLV-15` ; aucune réponse | Le digi ne répond qu'à l'indicatif tactique |
| Repli `F3` | Source `F4MLV-5` jamais entendue, chemin entrant invalide ; journal `Invalid RF path for F4MLV-5` ; ACK et réponse sur `WIDE1-1,RFONLY` | Repli sur `beacon.path` sans route connue |
| Message tiers `T1` | `}F4MLV-9>APRS,TCPIP,F4MLV-2*::F4MLV-15 :…{T1` ; ACK et réponse sur `WIDE1-1`, sans `RFONLY` | Comportement de passerelle conservé |
| `WIDE1-1` | Relayé en `F4MLV-15*,RFONLY` | Non-régression |
| `WIDE2-2` | Relayé en `F4MLV-15*,WIDE2-1,RFONLY` | Non-régression |
| `WIDE1-1,WIDE2-1` | Relayé en `F4MLV-15*,WIDE2-1,RFONLY` | Non-régression |
| ecoMode `E3` | Requête puis même requête 22 s plus tard : deux ACK, une seule réponse | Un ACK par réception et réponse unique après réveil |
| ecoMode `E4` | `{E4}A1` puis `{E4}A2` : `ackE4}A1`, `ackE4}A2`, une seule réponse | Reply-ack correct après réveil |
| `!RC1` | Commande `EM=OFF` signée par F4MLV-2 : `accepted at counter 20`, réponse `DigiEcoMode:OFF`, redémarrage hors ecoMode ; copie relayée au même compteur : `duplicate ignored at counter 20` et nouvel ACK `ack078` | Effet vérifié, copie sans seconde exécution |
| Upload APRS-IS | Copie directe puis copie `F4MLV-2*` de `UPLOAD-TEST-2300` : deux `Uploaded to APRS-IS` dans le journal de F4MLV-15 ; le serveur transmet une seule copie, `qAR,F4MLV-15` | Chaque réception est uploadée ; la déduplication reste au serveur |

### Interopérabilité avec le T-Deck — 23:15 UTC

Configuration initiale restaurée sur F4MLV-15 (iGate, digi 0, APRS-IS actif).
Le T-Deck (firmware Tracker) envoie `?APRSV{291` à `F4MLV-15` par
`WIDE1-1,WIDE2-1`.

| Heure | Observation |
| --- | --- |
| 23:15:04 | F4MLV-15 reçoit la copie directe et apprend `F4MLV-7 via DIRECT` |
| 23:15:06 | ACK `F4MLV-15>APLRG1,RFONLY::F4MLV-7  :ack291` ; le T-Deck échoue en réception (`code -7`) |
| 23:15:11 | Réponse émise ; nouvel échec `code -7` sur le T-Deck |
| 23:15:17 | F4MLV-15 reçoit la copie relayée `F4MLV-10,F6DEV-10*` |
| 23:15:20 | Nouvel ACK `ack291`, sans seconde exécution ; le T-Deck le reçoit et marque le message acquitté |

Conclusion limitée à l'essai : un client réel accepte l'ACK émis par
l'identité adressée sur la route directe apprise, et un ACK perdu est récupéré
par la copie suivante sans nouvelle exécution. La réponse à la requête n'a pas
été reçue par le T-Deck. La cause des erreurs `-7`
(`RADIOLIB_ERR_CRC_MISMATCH`) n'est pas déterminée.

Pendant cet essai, F4MLV-2, revenu à sa configuration d'iGate, a publié le
message sur APRS-IS ; F4MLV-15 l'a aussi traité par cette voie
(`Rx Query (APRS-IS)`), sans déduplication avec la réception RF. Ce
comportement existe upstream et sort du périmètre des correctifs.

### Interopérabilité avec Graywolf — 23:20 UTC

Graywolf, qui utilise F4MLV-2 comme radio, envoie `?APRSV{079` à `F4MLV-15`
par `WIDE1-1,WIDE2-1`. F4MLV-15 reçoit la copie directe (route `DIRECT`),
émet `ack079` puis la réponse sur `RFONLY`. Il reçoit ensuite une copie par
`F4MLV-10,F6DEV-10*` et émet un second `ack079`, sans seconde réponse.
Graywolf affiche le message sortant `acked` (une retransmission) et la réponse
reçue.

Conclusion limitée à l'essai : un second client réel accepte l'ACK et reçoit
la réponse. Graywolf a utilisé un numéro simple `{079}` ; le reply-ack avec un
client réel n'est pas exercé. Les lignes « Raw TNC-2 » affichées par Graywolf
sont reconstruites et diffèrent des trames réelles (destination, chemin,
destinataire) ; cet écart relève de Graywolf.

### Livraison physique isolée à deux relais — 23:51 UTC

Profils : A = 433,775 MHz SF12 CR5 ; B = 434,855 MHz SF9 CR7 ;
C = 434,300 MHz SF9 CR7, tous en 125 kHz.

| Appareil | Réception | Émission | Rôle |
| --- | --- | --- | --- |
| F4MLV-15 | B | A | iGate |
| F4MLV-2 | A | C | relais 1, digi mode 2 |
| F4MLV-10 | C | B | relais 2, digi mode 2 |
| F4MLV-7 (T-Deck) | B | B | terminal |

Le T-Deck ne peut entendre ni F4MLV-15 (A) ni F4MLV-2 (C). F4MLV-10 injecte
par son TNC le paquet d'apprentissage synthétique
`F4MLV-7>APLRT1,F4MLV-10,F4MLV-2*,RFONLY:>ROUTE-TEST-2`, puis `F4MLV-14`
envoie par APRS-IS le message `Test deux relais{D202` à F4MLV-7.

| Heure | Observation |
| --- | --- |
| 23:51:00 | F4MLV-15 : `Learned F4MLV-7 via F4MLV-2,F4MLV-10` |
| 23:51:11 | F4MLV-15 émet en A `F4MLV-15>APLRG1,F4MLV-2,F4MLV-10:}F4MLV-14>APRS,TCPIP,F4MLV-15*::F4MLV-7  :Test deux relais{D202` |
| 23:51:13 | Le T-Deck reçoit `…,F4MLV-2,F4MLV-10*:}…{D202`, seule copie reçue |
| 23:51:20 | Le T-Deck émet `ackD202` |
| 23:51:21 | F4MLV-15 reçoit l'ACK en B et le publie : `F4MLV-7>APLRT1,WIDE1-1,WIDE2-1,qAR,F4MLV-15::F4MLV-14 :ackD202` |

Conclusion limitée à l'essai : le chemin retour appris à deux relais est
exécuté par les deux relais physiques dans l'ordre, le message est livré au
terminal physique uniquement par le second relais, et l'ACK du terminal
revient à APRS-IS. Le chemin d'apprentissage est synthétique ; l'ACK revient
directement, sans relais.

Un premier essai (`D201`, 23:42 UTC) n'est pas retenu : le T-Deck était réglé
en B alors que le montage prévoyait C, et il a reçu la copie du relais 1.

### Point établi pendant la série

Un indicatif tactique désactive APRS-IS au démarrage
(`src/gps_utils.cpp:60-65`). Un message adressé à `Config.callsign` avec un
tactique configuré ne peut donc pas être traité par l'iGate : ce cas du plan
est sans objet. La WebUI affiche la valeur du fichier de configuration, et non
cette valeur forcée en mémoire.

### Limites de la série complémentaire

- Interopérabilité vérifiée avec le T-Deck et Graywolf, tous deux avec un
  numéro de message simple ; le reply-ack n'est vérifié qu'avec des trames
  injectées.
- F4MLV-15 n'a pas de carte SD.
- Les 3 variants publiés compilent (`ttgo-lora32-v21_SD`,
  `heltec_wifi_lora_32_V3_2`, `QRPLabs_LightGateway_Plus_1_0`) ; les autres
  environnements ne sont pas compilés.
- Avec l'alias `WIDE1-1` du repli, l'ACK de F3 a aussi été relayé par le digi
  public F5ZQC-10.
