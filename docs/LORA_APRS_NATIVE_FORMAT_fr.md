# Un format de trame natif pour LoRa APRS

## Pourquoi

LoRa APRS transporte de l'APRS texte, hérité d'un protocole conçu pour le VHF
à 1200 bauds il y a plus de 25 ans. Deux conséquences.

Le format est ambigu. Un commentaire peut être lu comme une altitude (`/A=`),
comme un PHG, comme un cap/vitesse, ou, depuis RXT, comme des mesures de
relais. Mic-E cache même des données dans le champ destination. Chaque
décodeur devine un peu différemment.

Le format est bavard. Les positions sont envoyées en texte ASCII, le chemin en
indicatifs complets, et en SF12 chaque octet coûte du temps d'antenne sur un
canal partagé par tout le réseau.

LoRa APRS est jeune, ses firmwares sont libres et reflashables, et quelques
projets couvrent l'essentiel du parc. C'est un bien meilleur terrain que le
VHF pour corriger ça.

## Principe

Garder l'APRS standard là où il est partagé, le changer là où il ne l'est
pas.

- Sur l'air, les stations LoRa utilisent un format natif : champs typés et
  délimités, encodage compact, aucune devinette.
- Vers APRS-IS, l'iGate traduit chaque trame en APRS texte standard. APRS-IS,
  aprs.fi et les utilisateurs VHF ne voient aucune différence.
- D'APRS-IS vers la RF (messages, objets), l'iGate traduit dans l'autre sens.

Les trames LoRa APRS actuelles commencent par l'en-tête `<\xFF\x01`. Le format
natif utiliserait `<\xFF\x02`. Les firmwares existants l'ignorent, les
nouveaux décodent les deux.

## Ce que ça apporte

- Une grammaire stricte : le commentaire libre est explicitement délimité et
  n'est jamais analysé pour y chercher des données cachées.
- Des trames plus courtes, donc moins de temps d'antenne par paquet et un
  canal moins chargé.
- De la place pour des extensions natives comme RXT, au lieu de les accrocher
  au commentaire.
- Des indicatifs de longueur libre, y compris les SSID alphanumériques
  (`-MC`, `-GS`).

## La seule contrainte dure

La traduction vers APRS-IS doit être entièrement déterministe. Deux iGates qui
reçoivent la même trame doivent produire exactement le même texte, octet pour
octet. Sinon APRS-IS ne reconnaît plus les doublons, et aprs.fi affiche deux
fois le même paquet, ce que les trailers RXT v1 provoquent déjà aujourd'hui.

## Ce qu'il faut modifier

**Spécification.** En-tête et version ; encodage de chaque type utile
(position avec cap, vitesse et altitude, statut, message, ACK et reply-ack,
télémétrie, météo, objet et item, requêtes, trames tierces) ; adressage et
chemin de relais (équivalent WIDEn-N, marqueur de saut utilisé) ; extensions
natives (RXT, commentaire libre délimité) ; traduction déterministe vers APRS
et traduction inverse ; vecteurs de test et corpus de référence.

**Bibliothèque de codec commune.** Encodage, décodage et traduction dans les
deux sens, en C++ embarquable avec tests hôte, utilisée à la fois par l'iGate
et le tracker. Graywolf (Go) la porte ou s'appuie sur les mêmes vecteurs de
test.

**Firmware iGate/digi.** Réception des deux en-têtes ; relais en manipulant
directement le chemin natif et RXT ; déduplication sur la trame décodée ;
traduction vers APRS-IS (q-construct, RFONLY et NOGATE compris) et d'APRS-IS
vers la RF (messages, objets, routage retour appris, reply-ack) ; réponses
locales (requêtes `?`, ACK, commandes `!RC1`) ; balises et télémétrie propres ;
toutes les sorties (KISS/TNC2 en texte traduit, MQTT, syslog, flux JSON,
journal SD, page des paquets reçus, tableau RXT) ; un réglage du format
d'émission (historique, natif, les deux).

**Firmware tracker.** Balises et messages au format natif, réception des
deux ; conversations, ACK et reply-ack, écran des trames brutes ; passerelles
vers les applications (TNC Bluetooth/KISS vers APRSdroid) en APRS texte
traduit ; digipeater intégré s'il est activé ; un réglage du format
d'émission.

**Graywolf.** Son transport LoRa TNC2 décode lui-même le format natif, ou
reçoit des trames déjà traduites par l'iGate ; encodage natif s'il émet
directement vers la LoRa.

**Transition et outils.** Gestion des deux formats tant que du matériel
historique est en service ; web flashers et documentation des trois projets ;
un décodeur en ligne de commande et des outils de capture comparant les
trames reçues à leur traduction APRS.

**Hors de notre portée.** Le firmware CA2RXU upstream, qui équipe l'essentiel
du parc LoRa APRS, et les autres firmwares LoRa APRS en circulation : sans
eux, le format natif reste limité aux réseaux qui l'adoptent.

## Comment ça pourrait se diffuser

Un réseau pilote qui fonctionne convainc davantage qu'une spécification. Le
réseau de Jon en Arizona utilise déjà ce fork et a lancé RXT : c'est un
terrain naturel pour essayer. Pour que ça se diffuse, trois conditions
doivent tenir :

- rien de visible pour les autres : un APRS standard et identique sur
  APRS-IS, sans doublons ni paquets rejetés sur aprs.fi ;
- des gains mesurés et publiés : temps d'antenne économisé, attribution RXT
  sans liste blanche, plus de commentaires tronqués, chiffres du réseau réel
  à l'appui ;
- une mise à jour sans risque : un nouvel iGate lit les deux formats dès le
  premier jour, pour être déployé sans casser les trackers déjà en service.

RXT v2 est un premier pas raisonnable dans cette direction : limité, testé
sur nos deux réseaux, et une façon de valider la méthode avant de proposer un
format complet.
