# Inventaire des équipements et déploiements

Ce document est la source de référence pour associer les noms usuels, les
indicatifs, les sites, les adresses réseau, les rôles et les firmwares. Une
information absente est indiquée comme inconnue et ne doit pas être déduite à
partir d'un autre équipement.

Dernière mise à jour : 28 septembre 2026.

## ThinkPad — poste de développement distant

- **Nom réseau :** `thinkpad`
- **Adresse Ethernet vérifiée :** `192.168.1.100/24` (`enp0s25`)
- **Adresse Wi-Fi vérifiée :** `192.168.4.2/24` (`wlp3s0`), sur le réseau/AP
  de F4MLV-10.
- **Accès SSH vérifié :**
  `ssh -o Hostname=thinkpad.local -o HostKeyAlias=thinkpad thinkpad`.
  Le nom `thinkpad` seul résout encore vers l'ancienne adresse
  `192.168.1.106`, non joignable ; `thinkpad.local` résout vers `.100`.
- **Routage vérifié :** route par défaut Ethernet via `192.168.1.1`, métrique
  100, prioritaire sur celle du Wi-Fi via `192.168.4.1`, métrique 600.
  Aucun routage inter-réseaux n'a été activé par l'agent.
- **Dépôt iGate actif :**
  `/home/fab2/Developpement/CA2RXU/LoRa_APRS_iGate`
- **Nature du dépôt actif :** clone Git du fork `moricef/LoRa_APRS_iGate`,
  branche `feature/reverse-message-routing`
- **Commit vérifié le 28 septembre 2026 :** `42a8a18` —
  `Implement learned reverse routing for APRS messages`
- **Sauvegarde de l'ancienne copie sans métadonnées Git :**
  `/home/fab2/Developpement/CA2RXU/LoRa_APRS_iGate.pre-git-20260928`

## Déploiement Firmin — F1ZDB-10

- **Nom usuel :** Firmin
- **Indicatif :** `F1ZDB-10`
- **Site physique :** Prat d'Albis, Ariège (09)
- **Coordonnées configurées :** `42.9204, 1.5883`
- **Rôle :** iGate APRS-IS et digipeater LoRa APRS fixe
- **LilyGo exécutant le firmware iGate :** `192.168.10.101` sur le réseau du
  site ; sa WebUI HTTP a été vérifiée à cette adresse le 28 septembre 2026
- **PC distant servant de point d'accès :** machine Linux `firmin`
- **Adresse VPN du PC :** `10.8.0.6` (`tun0`)
- **Adresse locale du PC :** `192.168.10.100` (`eno1`)
- **Accès au PC :** `ssh firmin` ; l'alias utilise l'utilisateur `firmin` et
  résout vers `10.8.0.6`
- **Accès distant à la WebUI du LilyGo :** établir par exemple le tunnel
  `ssh -L 8080:192.168.10.101:80 firmin`, puis ouvrir
  `http://127.0.0.1:8080/`
- **Endpoint Graywolf :**
  `http://127.0.0.1:18102/api/v1/aprs/stream`
- **Nature de cet endpoint :** adresse locale sur la machine qui exécute
  Graywolf ou son connecteur ; ce n'est pas une adresse permettant de joindre
  directement le PC Firmin ou le LilyGo depuis une autre machine
- **Lignée du firmware :** fork RXT/TNC2 local, et non firmware upstream pur
- **Routage retour appris :** absent du firmware actuellement identifié sur
  Firmin ; Firmin n'a pas reçu le firmware testé le 27 septembre
- **Build ou commit exact actuellement installé :** inconnu

La sauvegarde `images/iGateConfigurationBackup_Firmin.json` confirme notamment
la présence de RXT, de la liste blanche RXT et du protocole TNC2. Elle contient
des secrets et ne doit pas être copiée dans un rapport ou une réponse.

État de configuration communiqué le 28 septembre 2026 : APRS-IS réactivé,
balise RF activée, `messagesToRF` activé et syslog désactivé. Le chemin de
balise courant n'a pas été confirmé après ces modifications. La sauvegarde
antérieure contenait `WIDE2-1` et ne constitue pas une preuve de la valeur
actuelle.

## T-Deck — F4MLV-7

- **Nom usuel :** T-Deck
- **Indicatif :** `F4MLV-7`
- **Adresse de gestion :** `192.168.1.58`
- **Site physique :** inconnu dans les informations conservées
- **Rôle normal :** tracker physique
- **Rôle temporaire pendant les essais :** terminal physique pour `P201`, puis
  digipeater sur le profil Pologne pour l'essai à deux relais `P303`
- **Firmware observé le 28 septembre 2026 :** Tracker, build
  `2026-09-24 16:17:37 UTC` lu dans la WebUI ; commit exact inconnu.
- **Essais du 28 septembre :** récepteur de contrôle ; mode répéteur désactivé,
  aucun flash ni changement de configuration du T-Deck.
- **Accessibilité en fin d'essai :** HTTP sur `.58` ne répond plus ; la capture
  série confirme néanmoins des réceptions LoRa jusqu'à 20:44 UTC. Cause de
  l'indisponibilité HTTP non déterminée.

L'adresse `192.168.1.58` appartient au T-Deck/F4MLV-7. Elle ne doit jamais être
attribuée à Firmin/F1ZDB-10.

## iGate de test — F4MLV-15

- **Indicatif :** `F4MLV-15`
- **Rôle :** iGate utilisé pour les essais de routage retour
- **Adresse de gestion :** `192.168.1.165`
- **Firmware installé le 28 septembre 2026, 21:50 UTC :** variant
  `ttgo-lora32-v21_SD`, build `2026-09-28 21:49:19 UTC`, commit `36ac992`,
  confirmé dans la WebUI après OTA ; SHA-256 du binaire :
  `32e7c112bde9537ba1d30c7e516d1c8e52d71feb8b815a948b0466245a0db18c`.
  Il remplace le build `2026-09-28 19:34:05 UTC` (`a85b3d6`, SHA-256
  `eeb8f1a698f7201c0f23e3edb2fc9dfcb8c3f21c2c396f7e47e01640d9be45d6`).
  Configuration initiale sauvegardée dans
  `logs/validation-20260928/F4MLV-15-before.json` (contient des secrets).
- **Après la série complémentaire du 28 septembre :** configuration initiale
  restaurée et relue pour comparaison vers 23:05 UTC ; firmware `36ac992`
  conservé. Résultats : `docs/VALIDATION_CORRECTIFS_RF_20260928_fr.md`.
- **Site physique :** inconnu dans les informations conservées
- **Firmware pendant les essais du 27 septembre :** branche locale avec
  apprentissage et inversion du chemin RF de retour
- **Essais réalisés :** chemin appris par un relais, livraison physique avec
  ACK et exécution d'un chemin explicite à deux relais

Les résultats et leurs limites sont consignés dans
`docs/REVERSE_MESSAGE_ROUTING_fr.md`.

## Digipeater de test — F4MLV-2

- **Indicatif :** `F4MLV-2`
- **Rôle :** digipeater utilisé pendant les essais de routage retour
- **Adresse de gestion :** `192.168.1.161`
- **Firmware installé le 28 septembre 2026, 21:52 UTC :** même binaire que
  F4MLV-15 ci-dessus (`ttgo-lora32-v21_SD`, build `2026-09-28 21:49:19 UTC`,
  commit `36ac992`), date confirmée dans la WebUI après OTA. Configuration
  initiale sauvegardée dans `logs/validation-20260928/F4MLV-2-before.json`
  (contient des secrets).
- **Après la série complémentaire du 28 septembre :** configuration initiale
  restaurée et relue pour comparaison vers 23:05 UTC ; firmware `36ac992`
  conservé.
- **Site physique :** inconnu dans les informations conservées
- **Firmware pendant les essais du 27 septembre :** firmware de test de la
  branche locale de routage retour
- **Fonctions exercées :** répétition du chemin explicite à un relais et
  premier saut du chemin isolé à deux relais

## F4MLV-10

- **Indicatif :** `F4MLV-10`
- **Rôle établi :** digipeater RXT observé et utilisé sur le réseau
- **Autres rôles éventuels :** inconnus dans les informations conservées
- **Adresse de gestion locale/AP :** `192.168.4.1`
- **Accessibilité vérifiée le 28 septembre 2026 :** HTTP joignable depuis le
  ThinkPad en Wi-Fi ; accès depuis le poste de travail par SSH sur le
  ThinkPad. `/status` répond `OK`, et `/configuration.json` confirme
  `F4MLV-10`, sans indicatif tactique, digi mode 2, ecoMode 0.
- **Site physique :** inconnu dans les informations conservées
- **Firmware établi :** même firmware que F4MLV-15 et F4MLV-2, avec
  apprentissage du routage retour (information communiquée par l'utilisateur
  le 28 septembre 2026)
- **Build observé dans la WebUI :** `2026-09-28 10:21:26 UTC`, champ
  `Board / Environment: ttgo-lora32-v21` ; commit exact et variante de build
  précise inconnus. Aucune mise à jour de firmware pendant cette vérification.
- **Série complémentaire du 28 septembre :** émission coupée
  (`txActive=false`) de 21:53 à 23:03 UTC environ, pour qu'il ne participe pas
  aux essais. Configuration initiale
  (`logs/validation-complementaire-20260928/F4MLV-10-before.json`) restaurée et
  relue ensuite : `txActive=true`, digi mode 2. Firmware inchangé.
- **Essai à deux relais du 28 septembre :** reconfiguré temporairement vers
  23:34–23:55 UTC (profils de test hors 433,775 MHz, serveur TNC activé) comme
  relais 2 ; configuration initiale restaurée et relue ensuite.

## Réseau public de Jon — N7UV

- **Flux TNC public :** `n7uv1.duckdns.org:33001`
- **Accès brut :** `nc n7uv1.duckdns.org 33001`
- **Usage :** observer les paquets reçus et publiés par les équipements du
  réseau de Jon, notamment pour distinguer une absence de réception RF de
  BLACK d'une absence d'affichage dans la télémétrie RXT de PINAL
- **Durée d'observation :** ne pas imposer de délai court arbitraire ; les
  émissions recherchées peuvent être espacées
- **Sites cités dans les échanges :** `PINAL` et `BLACK`
- **Adresses de gestion et firmwares exacts :** inconnus dans les informations
  conservées

### Observation PINAL — BLACK du 28 septembre 2026

La capture conservée est
`/home/fab2/N7UV1_20260928_122532.log`. Elle contient notamment :

```text
BLACK>APLRG1,PINAL,ALAMOS*,WIDE2-1,qAR,N7AIL-14:=L=u0n2Im{# ...
```

Cette ligne conserve un chemin de BLACK passant par PINAL, mais aucun tuple
`PINAL<--BLACK RSSI:...` n'est présent dans ce fichier. Des valeurs
`PINAL<--BLACK RSSI:-71 SNR:+12.00` et `RSSI:-72 SNR:+1.75` ont été vues dans
le tampon d'une session `nc` antérieure, mais ce flux n'a pas été enregistré et
la sortie de l'outil a été tronquée. Elles doivent donc être considérées comme
une observation transitoire non reproductible, et non comme une preuve
archivée.

La capture conservée ne réfute pas le constat de Jon selon lequel PINAL
n'avait pas rapporté BLACK pendant les 21 heures qu'il examinait. Elle montre
seulement qu'un chemin BLACK via PINAL est apparu ultérieurement. La cause de
l'absence antérieure reste indéterminée.

## Identités de test qui ne sont pas des équipements déployés

- `F4MLV-MC` : identité synthétique utilisée comme source ou destination lors
  des essais ; ce n'était pas un récepteur physique.
- `F4MLV-14` : identité d'une connexion APRS-IS de test distincte ; elle ne
  désigne pas l'iGate RF qui effectuait les essais.

## Règles d'utilisation de cet inventaire

1. Ne jamais transférer une adresse IP, un rôle ou un firmware d'une ligne à
   une autre à partir de la proximité des échanges dans l'historique.
2. Distinguer une adresse de gestion d'équipement, une adresse de service
   locale et une identité APRS.
3. Ne pas considérer « firmware du fork », « firmware RXT » et « firmware de
   routage retour » comme synonymes.
4. Après chaque flash, enregistrer le commit ou la date de build exacte et la
   date du déploiement.
5. Après un changement réseau, mettre à jour séparément l'adresse de gestion et
   les éventuels endpoints de collecte ou de tunnel.
6. Si une valeur n'est pas consignée ou vérifiable, répondre qu'elle est
   inconnue au lieu de l'inférer.
