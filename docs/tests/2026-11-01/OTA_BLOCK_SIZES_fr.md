# Blocs OTA variables : essai matériel du 1 novembre 2026

[English](OTA_BLOCK_SIZES.md)

Les cinq tailles essayées ont livré le même échantillon de **8 192 octets**,
avec vérification SHA-256 du préfixe relu dans la partition inactive.
Un complément vérifie ensuite **128 et 175 octets par un relais** sur le
même échantillon. Ces essais valident le fonctionnement du banc ; ils ne
déterminent pas la taille optimale sur une liaison éloignée.
Les deux tailles sont ensuite réessayées avec un atténuateur sur MLV-15.
Un dernier complément utilise le montage annoncé avec 8,2 kΩ en série.

## Versions et montage

- Branche `experiment/lora-ota`, commit `d2349cc` ; cible
  `ttgo-lora32-v21-ota-a`, pont `bridge_fast` et client `block_bench.py`.
- Deux TTGO LoRa32 V2.1 avec SX1278 : MLV-2 comme pont USB/radio,
  MLV-15 comme cible. Aucun relais intermédiaire.
- Profil normal : 434,100 MHz, BW125, SF12, CR4/5, 2 dBm.
- Profil de transfert : 434,550 MHz, BW250, SF8, CR4/6, 3 dBm.
- Wi-Fi et point d’accès de la cible désactivés, état vérifié par radio.
- Antennes et disposition conservées pendant la série ; distance et
  atténuation non mesurées. Aucune atténuation contrôlée ni perte injectée.

Un passage par taille, ordre mélangé avec la graine `20261101` :
128, 175, 96, 204, puis 32 octets. Chaque passage repart de zéro.
Les pages flash restent de 4 096 octets pour toutes les tailles.

SHA-256 de l’échantillon commun :
`e0a770d1726664bb14211e7f51e9aaef3fa39dfa5f6d151dc3d7bffda5030492`.

SHA-256 du binaire exécuté par la cible :
`9582c8cbf85a939d0efcfbdb95933647040d41b90ee9593d5a3948461406dd63`.
L’empreinte ELF annoncée par radio correspond à celle du binaire conservé :
`a01331388424317447a7005f59dbd8b2b78357ae88dba8a58d8a8086180e50b7`.

## Observations en direct

| Bloc utile (octets) | Bitmap (octets) | Durée (s) | Temps d’antenne cumulé calculé (s) | Blocs retransmis | SHA-256 |
|---:|---:|---:|---:|---:|---|
| 32 | 16 | 45,84 | 41,49 | 1 | Conforme |
| 96 | 8 | 25,28 | 23,35 | 1 | Conforme |
| 128 | 4 | 22,02 | 20,47 | 0 | Conforme |
| 175 | 4 | 19,89 | 18,54 | 0 | Conforme |
| 204 | 4 | 19,39 | 18,12 | 0 | Conforme |

La durée commence avant WINDOW_START et se termine après la vérification
en flash. La négociation initiale, le retour au profil normal et la fermeture
de session sont hors de cet intervalle. Le temps d’antenne additionne les
émissions confirmées du pont et de la cible, avec leurs acquittements et
réparations. Il est calculé à partir des tailles et paramètres radio,
et non mesuré par un analyseur RF. Les lectures des compteurs sont exclues.

Les cinq passages ont conservé le même identifiant de démarrage de la
cible et ne signalent aucune erreur d’émission du pilote.

- Les blocs de 96, 175 et 204 octets exercent des fins de page plus courtes,
  sans faire traverser une limite de page à un bloc.
- Les blocs de 204 octets produisent des trames de 255 octets, maximum
  accepté en direct.
- Le passage de 96 octets a réparé le bloc d’indice 21 de la première page.
- Celui de 32 octets a réparé le bloc d’indice 77 : le bitmap de seize
  octets a donc servi à demander une réparation au-delà des 32 premiers
  blocs. L’origine RF de ces deux pertes n’est pas déterminée.

Le vérificateur d’échantillon n’a ni validé une image complète ni demandé
son activation. Chaque succès porte uniquement sur les 8 192 octets écrits
et relus, soit deux pages flash.

## Complément par MLV-10

Trajet sélectionné : **MLV-2 → MLV-10 → MLV-15**. Le relais utilise le
variant `ttgo-lora32-v21-ota-relay`, recompilé depuis `482c6a1` ; la cible,
le pont et l’échantillon sont identiques à ceux de l’essai direct. Le
profil de transfert reste 434,550 MHz/BW250/SF8/CR4/6, puissance demandée
3 dBm. La cible reste sans Wi-Fi.

Empreinte ELF du relais, vérifiée par son interface de diagnostic :
`08af58e8e0e37f237a402c6f58a6f2e971d8e75275da45199563f5da9191d9ce`.

Un passage par taille, dans l’ordre 175 puis 128 octets. Chaque passage
repart de zéro et vérifie le même SHA-256 des 8 192 octets relus en flash.
Le cache du relais reçoit une page, la retransmet à la cible, puis renvoie
son acquittement. Les blocs et leurs acquittements passent par radio ;
l’accès Wi-Fi de gestion sert à relever les compteurs du relais.

| Bloc utile (octets) | Durée (s) | Temps d’antenne cumulé calculé (s) | TX contrôleur / relais / cible | Blocs retransmis par le contrôleur | Réparations locales | SHA-256 |
|---:|---:|---:|---|---:|---:|---|
| 128 | 50,30 | 47,14 | 68 / 72 / 4 | 0 | 0 | Conforme |
| 175 | 45,12 | 42,49 | 52 / 56 / 4 | 0 | 0 | Conforme |

Aucun redémarrage de la cible ou du relais ni erreur TX du pilote n’est
signalé pendant ces deux passages. Ils vérifient les blocs de 175 octets
avec leur enveloppe de relayage, soit des trames de 255 octets, et leurs
fins de page plus courtes. Ils n’exercent pas les réparations locales,
puisqu’aucune n’a été nécessaire.

MLV-10 est sur le même site que les deux autres cartes, au bout de son
mât. Ce trajet à un relais n’est pas un essai à grande distance et ne
caractérise pas une liaison dégradée. Aucune image complète n’est activée.

## Complément avec atténuateur sur MLV-15

L’utilisateur a intercalé entre MLV-15 et son antenne un atténuateur en π :
deux résistances de 51 Ω à la masse et une de 820 Ω en série. Le calcul en
50 Ω donne 30,665 dB ; cette valeur n’a pas été mesurée à 434,550 MHz.
L’atténuateur avait été construit pour 431,975 MHz. Son insertion concerne
la liaison MLV-10 ↔ MLV-15 dans les deux sens.

Même fichier de 8 192 octets, mêmes binaires, même trajet et mêmes profils
que le complément précédent. Aucun reflash ni changement de configuration.
Un passage par taille, ordre 175 puis 128 octets ; aucune perte injectée.

| Bloc utile (octets) | Durée sans atténuateur (s) | Durée avec atténuateur (s) | Temps d’antenne cumulé recalculé avec atténuateur (s) | Blocs retransmis par le contrôleur | Réparations locales | SHA-256 |
|---:|---:|---:|---:|---:|---:|---|
| 128 | 50,30 | 51,43 | 48,12 | 1 | 0 | Conforme |
| 175 | 45,12 | 45,11 | 42,49 | 0 | 0 | Conforme |

Le passage de 128 octets a demandé au contrôleur le bloc d’indice 8 de la
première page, à l’offset 1 024. Il ne figurait pas dans le cache du relais.
Les compteurs donnent 65 envois de données par le contrôleur pour cette
taille, mais 64 blocs mis en cache et retransmis par MLV-10. Cette réparation
concerne donc le remplissage du cache sur le premier lien, pas une
réparation locale du lien atténué. La cause de la perte n’est pas établie.

Les TX contrôleur/relais/cible sont respectivement 70/74/5 en 128 octets
et 52/56/4 en 175 octets. Aucune erreur TX du pilote ni redémarrage n’est
signalé ; les deux SHA-256 relus en flash sont conformes. Aucun firmware
reçu n’est activé.

Ces deux passages ne font apparaître aucune réparation locale vers la
cible atténuée. Ils ne permettent pas de mesurer le point où les petits
blocs deviendraient avantageux. Ils ne mesurent pas non plus l’atténuation
effective du montage ni la marge RF disponible.

## Complément avec une résistance série nominale de 8,2 kΩ

L’utilisateur indique deux résistances de 51 Ω et une résistance série
nominale de 8,2 kΩ, avec environ 102 Ω mesurés entre les points chauds,
coax débranchés. Un π idéal avec ces composants donnerait 50,2 dB en 50 Ω.
L’atténuation RF effective de l’assemblage n’a pas été mesurée ; la mesure
à l’ohmmètre ne la caractérise pas.

Mêmes firmwares, fichier de 8 192 octets, trajet via MLV-10 et profils radio.
Un passage par taille, ordre 175 puis 128 octets, sans perte injectée.

| Bloc utile (octets) | Durée (s) | Temps d’antenne cumulé recalculé (s) | Blocs retransmis par le contrôleur | Réparations locales | SHA-256 |
|---:|---:|---:|---:|---:|---|
| 128 | 50,28 | 47,14 | 0 | 0 | Conforme |
| 175 | 45,11 | 42,49 | 0 | 0 | Conforme |

Les TX contrôleur/relais/cible sont 68/72/4 en 128 octets et 52/56/4 en
175 octets. Aucun redémarrage ni erreur TX pendant les mesures. Chaque
passage repart de zéro et vérifie les deux pages écrites par leur SHA-256 ;
aucune image reçue n’est activée.

Les nombres de trames et temps d’antenne recalculés sont identiques à ceux
du complément sans atténuateur. Sur ces deux passages, aucun régime de
pertes sur le lien atténué n’est mis en évidence. On ne peut en déduire ni
une atténuation effective de 50,2 dB, ni une marge de réception, ni un seuil
de distance où les petits blocs seraient préférables. Avant d’attribuer
les résultats à un niveau d’affaiblissement, il faut mesurer ce niveau.

## Vérification préliminaire par RSSI, avec puis sans le montage

Trois requêtes APRS de version ont provoqué chacune un ACK et une réponse
de GENATX (MLV-15), enregistrés par MLV-10. Le RSSI provient du récepteur
de MLV-10, en filtrant l’émetteur GENATX et les réceptions récentes.
La mesure utilise le profil normal du banc : **434,100 MHz, BW125, SF12,
CR4/5, puissance configurée de MLV-15 2 dBm**. Ce ne sont pas les trames
OTA sur le profil rapide à 434,550 MHz.

| Montage annoncé par l’utilisateur | RSSI ACK (dBm) | RSSI réponse de version (dBm) | SNR ACK / réponse (dB) |
|---|---:|---:|---|
| Montage annoncé avec 8,2 kΩ en série | −86 | −86 | 10,75 / 8 |
| Antenne raccordée directement, atténuateur retiré | −85 | −85 | 12,75 / 11,25 |
| Montage annoncé avec 82 kΩ en série | −86 | −87 | 11,25 / 8,75 |

L’écart moyen observé par rapport à l’antenne directe est d’environ
**1 dB avec 8,2 kΩ et 1,5 dB avec 82 kΩ**. Le π idéal de 82 kΩ avec les
deux 51 Ω donnerait environ 70 dB en 50 Ω ; ces relevés ne démontrent pas
cet affaiblissement sur la liaison. Il s’agit de deux
trames par condition, pas d’une caractérisation calibrée de l’atténuateur.
Ces relevés ne démontrent pas un affaiblissement important de cette liaison
par le montage actuel. Ils ne permettent pas d’en attribuer la cause à un
composant précis : le chemin RF, le câblage et d’éventuels couplages restent
à vérifier avant d’augmenter encore la résistance série.

Traces et relevés : `logs/attenuator-rssi-20261101/`, conservés localement.

## Correction du calcul du temps d’antenne

Les temps d’antenne des deux tableaux remplacent les premières estimations.
Sur SX1278 avec RadioLib 7.6.0, le passage BW125/SF12 → BW250/SF8
appliquait la bande passante avant le SF. La bibliothèque désactivait
correctement l’optimisation LDRO dans le registre matériel en changeant le
SF, mais conservait une ancienne valeur dans son état utilisé par
`getTimeOnAir()`. Les compteurs surestimaient donc la durée des émissions.

Les valeurs corrigées sont recalculées avec le profil final, préambule de
huit symboles, en-tête explicite et CRC actif. Les longueurs sont obtenues
des traces d’émission et des formats d’ACK ; leurs nombres et leurs sommes
d’octets concordent avec les compteurs de la cible et du relais. Le même
calcul avec l’état LDRO périmé reproduit exactement leurs compteurs bruts
pour les onze passages. Pour le passage de 128 octets avec l’atténuateur de 820 Ω,
le cache ne retransmet qu’une fois chaque offset de données ; le doublon
émis par le contrôleur reste compté sur son propre lien. Les nombres de
trames et d’octets sont recoupés séparément pour les trois émetteurs.
Les traces originales sont conservées sans modification.

Le correctif applique le SF avant la bande passante pour actualiser aussi
l’état de calcul de RadioLib. Il compile pour la cible, le relais et le
pont ; les 34 tests Python passent. Ce correctif n’a pas été reflashé
pendant ces essais : ses compteurs restent à contrôler sur carte. La
correction ne change ni les durées chronométrées ni les SHA-256 obtenus.

## Portée et suite

Le banc peut maintenant servir aux séries comparatives décrites dans le
[protocole d’évaluation](../../proposals/LORA_OTA_BLOCK_BENCH_fr.md).
Les durées ci-dessus décrivent onze passages isolés, pas une distribution
statistique. Elles ne justifient pas un changement du bloc par défaut.

Restent à comparer les tailles avec répétitions et plusieurs niveaux
d’atténuation documentés, en direct et par relais. Une validation de portée
sur un site éloigné reste distincte de cette campagne sur place. Les petits
blocs peuvent devenir avantageux si les blocs longs demandent davantage de
réparations ; ces essais ne déterminent pas ce seuil. Les bitmaps de douze octets,
les reprises après panne avec blocs variables et une image complète dans
cette nouvelle géométrie ne sont pas validés par cet essai.

Les traces, résultats CSV/JSON, binaires et empreintes sont conservés dans
`logs/block-smoke-20261101/`, `logs/block-relay-20261101/` et
`logs/block-relay-pad-20261101/` et `logs/block-relay-pad82k-20261101/`,
dossiers locaux ignorés par Git.
