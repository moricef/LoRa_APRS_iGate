# Commandes RXT et profils OTA par radio — 1 novembre 2026

## Objet et périmètre

Vérifier les effets réels des commandes signées : ajout ou absence de
mesure RXT lors d'un relayage, sauvegarde explicite après redémarrage,
échanges radio sur le profil OTA temporaire et retour au profil habituel.
Cet essai ne transfère aucune image de firmware et ne mesure ni portée,
ni débit, ni puissance RF.

## Montage et versions

- Deux TTGO LoRa32 V2.1 équipées de SX1278, en liaison directe sur le banc.
- MLV-2 : pont USB programmable `bridge_fast`, relié au programme de test.
- MLV-15 : firmware iGate complet OTA-A ; Callsign-SSID `F4MLV-15`,
  identité radio `GENATX`, contrôleur autorisé `F4MLV-2`.
- Sources de `experiment/lora-ota` au commit `680451d` ; commandes RXT et
  admission des commandes OTA par l'authentificateur ajoutées dans `dcaca1e`.
- Image OTA-A recompilée intégralement : SHA-256
  `723ea0979a289714415003d751f4ae76d9b198c43a3b334f2891b622ec1b3339`.
- Profil habituel de cet essai : 434,100 MHz, BW125, SF12, CR4/5,
  puissance demandée de 2 dBm.
- Profil temporaire : 434,550 MHz, BW250, SF8, CR4/6,
  puissance demandée de 3 dBm.

La cible est configurée sans Wi-Fi ni point d'accès. L'USB de la cible
sert au provisionnement de la clé de laboratoire, à l'observation du
journal et aux redémarrages ; il ne transporte pas les commandes APRS.
Le pont émet les messages signés `!RC1` et observe les réponses par LoRa.
Le programme utilise le même domaine HMAC et les mêmes champs signés que
Graywolf, mais ne passe pas par son interface ni son transport TNC2.
Il complète donc l'essai manuel depuis Graywolf du 31 octobre.

## RXT : observations

1. `RXT=ON`, suivi de `COMMIT`, enregistre l'état ON. Un `RXT=OFF` sans
   `COMMIT` est ensuite perdu au redémarrage : `RXT=?` répond ON.
2. `RXT=OFF`, suivi de `COMMIT` puis d'un redémarrage, reste OFF.
3. Dans cet état OFF, une trame texte de test `F4MLV-9>APLRT1,WIDE2-1`
   est reçue par le pont après relayage sous le chemin `GENATX*`.
   Elle ne contient pas de bloc RXT caché.
4. `RXT=ON`, suivi de `COMMIT` puis d'un redémarrage, reste ON.
5. Une autre trame de test, avec un contenu distinct, est reçue après
   relayage sous `GENATX*`, avec le séparateur `00 01` et exactement cinq
   octets de contenu RXT : empreinte du relais et quadruplet local.
6. Une commande `RXT=OFF` signée avec une mauvaise clé reçoit `AUTH FAILED`.
   La requête correctement signée suivante confirme que RXT reste ON.

Le pont enregistre les octets radio reçus. Les observations ON/OFF ne
reposent donc pas uniquement sur le texte de la réponse de commande.
Ces sondes n'avaient aucune mesure RXT préalable : la conservation de
mesures d'autres relais n'est pas réévaluée ici.

## Profil temporaire : observations

1. `OTA=PROFILE,434550000,250000,8,6,3` reçoit `OTA PROFILE READY (RAM)`.
2. `OTA=FAST,30` reçoit `OTA FAST IN 2S LEASE=30` sur le profil habituel.
   Le pont adopte ensuite le profil temporaire.
3. Sur ce profil, une nouvelle requête signée `OTA=?` reçoit par radio
   `OTA FAST ARMED=0 PROFILE=1`. L'aller-retour est donc effectif sur
   les paramètres temporaires des deux radios.
4. `OTA=NORMAL` est envoyé et reçoit `OTA RETURN NORMAL` sur ce profil.
   Le pont revient alors au profil habituel ; la requête suivante reçoit
   `OTA NORMAL ARMED=0 PROFILE=1`.
5. Une demande de profil SF5 est refusée avec `OTA UNSUPPORTED PROFILE`.
   La cible SX1278 reste joignable sur le profil habituel.
6. Une nouvelle bascule avec `OTA=FAST,15` permet encore une requête et
   une réponse sur le profil temporaire. Après silence et expiration du
   bail, une requête sur le profil habituel reçoit une réponse NORMAL.
7. Après une autre bascule et un échange sur le profil temporaire, un
   redémarrage de la cible est provoqué par son port série. Elle répond
   ensuite sur le profil habituel avec `PROFILE=0` : le profil préparé,
   conservé seulement en RAM, a disparu.

Les paramètres radio sont ceux demandés aux transceivers. Leur fréquence
exacte et leur puissance de sortie n'ont pas été mesurées par un instrument.
Le test d'expiration vérifie la joignabilité après le bail ; il ne fournit
pas une nouvelle mesure précise du délai de retour.

## Conclusion et limites

Les commandes signées pilotent effectivement l'ajout local de RXT et le
profil temporaire. La sauvegarde RXT par `COMMIT`, la perte d'un changement
non sauvegardé, l'aller-retour APRS sur le profil temporaire et les retours
explicite, après silence et après reset sont vérifiés sur les cartes.

Ces résultats portent sur un équipement piloté en liaison directe. Ils
ne valident pas une bascule APRS coordonnée à travers plusieurs relais,
une nouvelle image complète, une activation de firmware, une panne pendant
une écriture flash/NVS, ni l'usage sur un site éloigné. Les validations
précédentes du transfert complet restent distinctes.

Les sauvegardes, les clés, les scripts et les captures sont conservés
localement dans `logs/aprs-profile-final-20261101/`, hors publication.
