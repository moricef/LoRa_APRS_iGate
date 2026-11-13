# Transfert OTA d'une image complète par un relais — essai RF du 13 novembre 2026

[English version](FULL_IMAGE_RELAYED_OTA_VALIDATION_20261113_en.md)

> Compte rendu d'essai. Les conclusions portent uniquement sur le montage,
> les versions et les conditions décrites ci-dessous.

## Objectif

Vérifier qu'une image de firmware complète peut être transférée par radio de
F4MLV-2 vers F4MLV-15, via F4MLV-10, malgré une liaison atténuée. L'essai doit
aussi démontrer la reprise après une perte de liaison, la vérification de
l'image dans la partition inactive et le retour au profil radio de contrôle.
Pour cet essai, l'image est seulement écrite et vérifiée : son activation, son
démarrage et un éventuel retour à l'image précédente restent hors périmètre.

## Montage et paramètres

- contrôleur et pont USB : F4MLV-2 ;
- relais radio : F4MLV-10 ;
- cible : F4MLV-15, alimentée et active en radio, mais avec le Wi-Fi et le
  point d'accès désactivés ;
- cascade d'atténuation du 8 novembre maintenue entre F4MLV-15 et son
  antenne ;
- même firmware iGate OTA unifié sur les trois stations : `4.0.2RXT-OTA`,
  build `2026-11-11 15:00:28 UTC` ;
- image complète non compressée : 1 782 064 octets, SHA-256
  `020e39263efa5d4fc0e5d06f88edc83b0c47928801a8155c5269292a137b0f86` ;
- profil de contrôle, utilisé pour ouvrir la session et après expiration du
  bail : 434,100 MHz, BW 125 kHz, SF12, CR 4/5, 20 dBm ;
- profil de transfert : 434,500 MHz, BW 500 kHz, SF7, CR 4/5, 9 dBm ;
- blocs de 160 octets, regroupés en pages de 4 096 octets ; le point de
  reprise n'avance qu'après l'écriture complète d'une page en flash ;
- commande d'activation non envoyée.

### Cascade d'atténuation

La cascade comprend deux cellules en π montées à la suite. Chaque cellule
utilise une résistance de 820 Ω en série et deux résistances de 51 Ω à la
masse, soit au total deux résistances de 820 Ω et quatre de 51 Ω.

Son insertion avait été mesurée à **53 dB au NanoVNA**. Sur la liaison radio
du banc, la différence observée avec et sans cascade n'était toutefois que de
**28 dB**. Lors de cette mesure antérieure, MLV-15 réglé à 2 dBm était reçu
par MLV-10 à **-113 dBm**, avec un SNR de **+3,75 dB**. Le montage physique
est resté inchangé pour le transfert complet, mais ces valeurs n'ont pas été
remesurées le 13 novembre. Les 53 dB du NanoVNA décrivent donc le composant
mesuré, tandis que les 28 dB caractérisent l'écart effectivement observé sur
le banc radio précédent ; ils ne sont pas interchangeables.

Le client hôte est `tools/lora_ota/send_integrated.py`. La reprise après
expiration du bail est celle du commit `e559036`. L'ouverture du pont intégré
iGate par `OTABRIDGE` et sa fermeture par `BRIDGE OFF`, utilisées pendant cet
essai, étaient encore des modifications locales non commitées.

## Résultats

La cible a d'abord confirmé que son Wi-Fi était coupé et qu'aucune interface
réseau n'était active. Le profil rapide a ensuite été négocié et le transfert
a commencé à l'offset zéro.

Une perte de liaison est survenue après 831 488 octets écrits durablement.
Le client a attendu l'expiration du bail de 60 secondes, rouvert une session
par `HELLO` sur le profil normal, renégocié le profil rapide puis présenté de
nouveau le manifeste. La cible a repris exactement à l'offset 831 488 et le
transfert s'est poursuivi jusqu'au bout.

| Mesure | Résultat |
| --- | ---: |
| Octets vérifiés | 1 782 064 |
| Pages validées | 436 |
| Cycles de renvoi de blocs manquants | 15 |
| Requêtes de contrôle répétées après absence de réponse | 9 |
| Pertes de liaison | 1 |
| Temps de reprise cumulé | 103,5 s |
| Durée du début du transfert à la vérification | 3 608,2 s |
| Débit utile moyen, reprise comprise | 493,9 octets/s |

La cible a relu l'image complète dans sa partition inactive et confirmé son
SHA-256. Le client a ensuite confirmé le retour au profil de contrôle. Aucune
commande d'activation n'ayant été envoyée, l'image est restée dans la
partition inactive de F4MLV-15 et n'a pas été démarrée.

Après l'essai, F4MLV-2 et F4MLV-10 ont été relus sur leur configuration de
service à 433,775 MHz. La configuration de F4MLV-10 était identique à sa
sauvegarde préalable : digi 2 et balises RF actives. F4MLV-15 est revenu à
son profil enregistré de laboratoire à 434,100 MHz. La cascade est restée en
place.

## Conclusion et limites

Dans ce montage précis, l'essai démontre un transfert complet, la conservation
de l'avancement après une perte de liaison, la reprise sur une nouvelle
session et la vérification de l'image reçue par une cible sans accès réseau,
derrière un relais.

Il ne valide ni l'activation, ni le démarrage du firmware reçu, ni le retour à
l'image précédente. Il s'agit d'un seul passage complet sur un trajet et un
montage d'atténuation donnés. La négociation et la fermeture du pont iGate
côté client doivent encore être commitées avant d'être considérées comme
publiées ; la logique de reprise elle-même est déjà commitée.

Les journaux complets sont conservés localement dans
`LoRa_APRS_iGate-ota/logs/ota-full-image-cascade-20261113-191807Z/`.
