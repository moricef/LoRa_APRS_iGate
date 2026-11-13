# Transfert OTA d'une image complète par un relais — essai RF du 13 novembre 2026

[English version](FULL_IMAGE_RELAYED_OTA_VALIDATION_20261113_en.md)

> Compte rendu d'essai. Les conclusions portent uniquement sur le montage,
> les versions et les conditions décrites ci-dessous.

## Objectif

Vérifier qu'une image de firmware complète peut être transférée par radio de
F4MLV-2 vers F4MLV-15, via F4MLV-10, malgré une liaison atténuée. L'essai doit
aussi démontrer la reprise après une perte de liaison, la vérification de
l'image dans la partition inactive et le retour au profil radio normal. Le
firmware reçu ne doit pas être activé.

## Montage et paramètres

- contrôleur et pont USB : F4MLV-2 ;
- relais radio : F4MLV-10 ;
- cible hors ligne : F4MLV-15 ;
- cascade d'atténuation du 8 novembre maintenue entre F4MLV-15 et son
  antenne ;
- même firmware iGate OTA unifié sur les trois stations : `4.0.2RXT-OTA`,
  build `2026-11-11 15:00:28 UTC` ;
- image complète non compressée : 1 782 064 octets, SHA-256
  `020e39263efa5d4fc0e5d06f88edc83b0c47928801a8155c5269292a137b0f86` ;
- profil de contrôle : 434,100 MHz, BW 125 kHz, SF12, CR 4/5, 20 dBm ;
- profil de transfert : 434,500 MHz, BW 500 kHz, SF7, CR 4/5, 9 dBm ;
- blocs de 160 octets et pages durables de 4 096 octets ;
- activation désactivée explicitement.

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

La cible a d'abord confirmé qu'elle était hors ligne. Le profil rapide a
ensuite été négocié et le transfert a commencé à l'offset zéro.

Une perte de liaison est survenue après 831 488 octets écrits durablement.
Le client a attendu l'expiration du bail de 60 secondes, rouvert une session
par `HELLO` sur le profil normal, renégocié le profil rapide puis présenté de
nouveau le manifeste. La cible a repris exactement à l'offset 831 488 et le
transfert s'est poursuivi jusqu'au bout.

| Mesure | Résultat |
| --- | ---: |
| Octets vérifiés | 1 782 064 |
| Pages validées | 436 |
| Cycles de réparation | 15 |
| Répétitions de requêtes | 9 |
| Pertes de liaison | 1 |
| Temps de reprise cumulé | 103,5 s |
| Durée rapportée du transfert | 3 608,2 s |
| Débit utile moyen, reprise comprise | 493,9 octets/s |

La cible a accepté la vérification finale de l'image. Le client a ensuite
confirmé le retour au profil normal et l'absence d'activation. L'image reste
donc dans la partition inactive de F4MLV-15.

Après l'essai, F4MLV-2 et F4MLV-10 ont été relus sur leur configuration de
service à 433,775 MHz. La configuration de F4MLV-10 était identique à sa
sauvegarde préalable : digi 2 et balises RF actives. F4MLV-15 est revenu à
son profil enregistré de laboratoire à 434,100 MHz. La cascade est restée en
place.

## Conclusion et limites

Cet essai valide un transfert complet, la conservation de l'avancement après
une perte de liaison, la reprise sur une nouvelle session et la vérification
de l'image reçue par une cible hors ligne derrière un relais.

Il ne valide ni l'activation, ni le démarrage du firmware reçu, ni le retour à
l'image précédente. Il s'agit d'un seul passage complet sur un trajet et un
montage d'atténuation donnés. La négociation et la fermeture du pont iGate
côté client doivent encore être commitées avant d'être considérées comme
publiées ; la logique de reprise elle-même est déjà commitée.

Les journaux complets sont conservés localement dans
`LoRa_APRS_iGate-ota/logs/ota-full-image-cascade-20261113-191807Z/`.
