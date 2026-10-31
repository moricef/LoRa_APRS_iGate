# Profils OTA configurés et activés par RF — 31 octobre 2026

## Objet

Vérifier qu'un profil de transfert choisi par le contrôleur est transmis
et activé par commande RF authentifiée, sans remplacer la configuration
radio habituelle des cartes. Le transfert est borné à quatre pages et ne
répète pas la validation d'une image complète du 30 octobre.

## Montage et versions

- MLV-2 : pont USB programmable ; MLV-10 : relais unique ; MLV-15 : cible
  OTA-A, Wi-Fi et point d'accès désactivés, passage obligatoire par le relais.
- Trois TTGO LoRa32 V2.1 avec SX1278. Branche `experiment/lora-ota`, base
  `3dc297c` avec les modifications locales de profils du 31 octobre.
- Profil habituel du banc : 434,100 MHz, BW125, SF12, CR4/5.
- Profil choisi pour cet essai : 434,550 MHz, BW250, SF8, CR4/6,
  puissance de 3 dBm demandée au transceiver ; bail de 30 secondes.
- Manifeste de l'image B : 1 766 368 octets, SHA-256
  `b35e268e4054d4e26b4cbed140690ed2c681fcad14ee2151c00c43eeb6372b69`.
- Firmware relais : 1 755 360 octets, SHA-256
  `03c26fd63d4ad79c95eca671778b046db4b0f9b7dccc563b64c5dc2d088fa06d`.

Le pont reçoit ses deux définitions de profil par USB depuis le fichier
JSON du contrôleur. Le profil temporaire du relais et de la cible est
transmis par RF dans PREPARE_FAST, sélecteur 2. Les clés et les rôles sont
provisionnés avant l'essai. La lecture HTTP des compteurs du relais sert
au diagnostic ; elle ne transporte ni le profil temporaire ni l'image à
la cible.

## Observations

1. Une demande RF avec SF5 est rejetée par le relais SX1278. Son compteur
   de refus augmente d'une unité, son profil reste habituel et la cible
   répond encore sur ce profil. Le relais ne transmet pas cette demande.
2. La demande valide est acquittée, puis les échanges et le transfert
   fonctionnent sur 434,550 MHz/BW250/SF8/CR4/6, différent de l'ancien
   profil fixé à 434,500 MHz/BW500/SF7/CR4/5.
3. Quatre pages, soit 16 384 octets, sont enregistrées. WINDOW_FINISH et
   WINDOW_ACTIVATE refusent l'image incomplète.
4. Le contrôleur revient localement au profil habituel et laisse les cartes
   sans trafic pendant 35 secondes. Une nouvelle session authentifiée
   aboutit sur le profil habituel ; WINDOW_START retrouve les 16 384 octets
   durables. Le journal n'a donc pas été perdu au retour automatique.
5. Après annulation et réarmement, le même profil temporaire est activé
   puis RETURN_NORMAL rétablit la communication sur le profil habituel.
6. L'ancien corps de PREPARE_FAST, sélecteur 1, est également accepté :
   passage à son profil fixe historique, échanges radio, puis retour normal.
   Ceci vérifie sa compatibilité avec les nouveaux firmwares, sans faire
   intervenir un ancien firmware dans le montage.
7. L'application exécutée, la partition de démarrage et l'absence de
   confirmation en attente restent inchangées. La cible confirme par radio
   que le Wi-Fi est désactivé et qu'aucune interface IP n'est active.
8. Après arrêt du transfert, une lecture indépendante des 16 384 octets
   de la partition inactive confirme leur identité avec le fichier B.
   SHA-256 du préfixe :
   `99c2f66995f2eac95310da4c796c47879062dfc214c48fddfe45cce63279544a`.
9. Après le reset provoqué par cette lecture, un contrôle radio confirme
   à nouveau la réponse sur le profil habituel, la même application OTA-A
   et le Wi-Fi désactivé. Le profil temporaire n'est pas devenu le profil
   de redémarrage.

## Conclusion et limites

Cet essai vérifie la configuration et l'activation du profil temporaire
par RF avec un relais, son usage pour un transfert court, le refus d'un
profil incompatible et les retours après silence, commande et reset.
Il complète les essais de reprise et de transfert complet précédents.

Il ne valide pas une nouvelle image complète, une liaison terrain, d'autres
puces radio ou SF5 sur un matériel compatible. La puissance demandée n'a
pas été mesurée à la sortie RF. Les profils habituels des cartes restent
provisionnés avant l'essai ; leur modification permanente par RF ne fait
pas partie de cette extension. Toute la chaîne utilise les nouveaux
firmwares pour le sélecteur 2.

Les quatre suites C++ et les 23 tests Python passent. Les variantes OTA-A,
OTA-B, relais, iGate standard et le pont `bridge_fast` compilent. Les
artefacts, scripts de mesure et journaux sont conservés dans
`logs/lora-ota-configurable-20261031/`, hors publication.
