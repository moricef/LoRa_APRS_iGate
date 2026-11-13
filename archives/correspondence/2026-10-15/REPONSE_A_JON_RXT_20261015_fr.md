---
title: "Réponse à Jon — RXT v2 et télémétrie réseau"
author: "Fabrice F4MLV"
date: "15 octobre 2026"
lang: fr-FR
---

# Ce qui est actuellement déployé

La version du 13 octobre 2026 est toujours identifiée comme **4.0.2RXT**, mais
elle contient trois évolutions importantes : RXT v2, un transport RXT caché
compatible avec les trames APRS classiques et un format LoRa APRS natif encore
expérimental.

Pour les relais destinés au terrain, je recommande actuellement le **mode
texte**, qui est le réglage par défaut. La trame APRS reste alors une trame
texte normale et les mesures RXT voyagent dans un bloc caché après un octet
nul. Un firmware CA2RXU classique ne voit que la partie APRS et continue donc
à fonctionner normalement. Les données RXT ne sont jamais ajoutées au
commentaire envoyé vers APRS-IS.

Le format natif binaire réduit le temps d’occupation radio et transporte les
mesures de façon plus précise, mais il doit encore être considéré comme un
format d’essai. Je ne le choisirais pas aujourd’hui pour des relais installés
sur des sommets inaccessibles pendant l’hiver.

# Ce que signifie la réception directe depuis SOMTNP

Quand une trame est répétée par SOMTNP, celui-ci mesure le signal qu’il a reçu
du nœud précédent. Il ajoute quatre informations :

- le RSSI ;
- le SNR ;
- le décalage de fréquence ;
- le délai total avant la fin de sa retransmission, appelé TTH.

Recevoir RXT directement depuis SOMTNP signifie donc que l’on ne voit plus
seulement la qualité du dernier signal reçu par l’iGate. On dispose aussi de
la mesure effectuée sur le site de SOMTNP, pour la liaison qui précède ce
relais.

Sur un chemin comportant plusieurs relais RXT, chaque relais peut ajouter sa
propre mesure. On peut alors distinguer les différents tronçons du parcours et
identifier celui qui se dégrade : signal faible, bruit, décalage de fréquence
ou attente importante avant retransmission.

C’est l’intérêt principal pour le réseau : diagnostiquer les liaisons depuis
les sommets sans devoir se rendre physiquement sur chaque site.

# Pourquoi RXT v2 est différent

RXT v1 ne disait pas explicitement quel relais avait produit chaque mesure.
L’iGate devait le déduire à l’aide d’une liste blanche locale. Si cette liste
était différente ou incomplète, une mesure pouvait être attribuée au mauvais
relais.

RXT v2 ajoute à chaque mesure une courte empreinte calculée depuis l’indicatif
du relais. La liste blanche n’est plus nécessaire. Si une collision empêche
une attribution certaine, le firmware signale l’ambiguïté au lieu de deviner.

Le correctif du 13 octobre va plus loin pour les trames natives : il conserve
en mémoire l’association exacte entre la mesure et l’indicatif complet. Cette
association reste correcte lorsque le chemin est réécrit et lorsque plusieurs
paquets attendent simultanément dans la file de retransmission.

Ce comportement a été vérifié par radio entre deux TTGO. Le test utilisait
volontairement deux indicatifs ayant la même empreinte, avec une mesure sur un
seul des deux. La mesure est restée attachée au bon indicatif après la
suppression d’un alias consommé. Un second test a conservé un paquet sept
secondes dans la file pendant la réception d’un autre paquet ; chacun a gardé
ses propres mesures.

# Interprétation de RX_rate, RelRate et DrpRate

Ces valeurs sont des taux en paquets par heure :

| Valeur | Signification |
|---|---|
| `RX_rate` | Paquets LoRa APRS valides acceptés par le récepteur |
| `RelRate` | Paquets acceptés et placés dans la file du digipeater |
| `DrpRate` | Paquets refusés par une règle : liste noire, doublon, chemin, retour vers soi-même ou `NOGATE` |

Ce ne sont pas les trois termes d’un même bilan. Un iGate peut accepter un
paquet sans devoir le répéter. Un paquet de la liste noire augmente au
contraire `DrpRate` sans augmenter `RX_rate`. Il est donc normal que
`RX_rate` soit différent de `RelRate + DrpRate`.

Les compteurs sont convertis en taux horaires selon la durée réelle depuis le
rapport précédent. Après un démarrage, une fenêtre courte peut donner un pic
temporaire : dix paquets observés pendant quinze minutes deviennent un taux de
quarante paquets par heure.

Les messages `EQNS` n’ont pas multiplié ces valeurs. Les coefficients de ces
trois canaux sont `0,1,0`. Le pic d’environ quatre fois observé après le
remplacement de SOMTNP peut donc correspondre à une première fenêtre proche de
quinze minutes, à une hausse réelle du trafic, ou aux deux. Il faut comparer
les heures exactes des rapports pour le déterminer.

# Recommandation pour OATMNN, TLGRPH et les futurs sites

Pour OATMNN et TLGRPH, j’utiliserais la même version RXT v2 du 13 octobre et le
mode de transmission texte par défaut. Une mise à jour coordonnée des relais
RXT est préférable : un équipement ancien continuera à transporter l’APRS,
mais il peut perdre les mesures cachées et ne permettra pas une chaîne RXT
complète.

Pour les trois futurs sites QRP Labs, la variante existe et compile, mais le
correctif du 13 octobre n’a été vérifié sur matériel qu’avec deux TTGO proches.
Une compilation réussie ne démontre pas plusieurs mois de stabilité sur un
QRP Labs.

Avant une installation en altitude, je ferais donc fonctionner chaque unité
QRP Labs plusieurs jours avec son alimentation, sa configuration et une
activité radio réaliste. Il faut surveiller les redémarrages, la mémoire, les
files de retransmission, RXT et les reconnexions réseau. Cette attente est
justifiée par l’impossibilité probable d’intervenir sur les sites pendant
l’hiver, et non par un défaut RXT déjà identifié sur les QRP Labs.

Le redémarrage du Heltec toutes les dix à quinze minutes doit être étudié
séparément à partir de la cause de reset et du journal série. Il ne permet pas,
à lui seul, de conclure que le même problème existe sur les QRP Labs.

# En résumé

SOMTNP fournit maintenant une mesure du tronçon radio qu’il reçoit, et pas
seulement une copie du paquet. Avec plusieurs relais RXT v2, nous pouvons
observer chaque étape du chemin, sans liste blanche et sans modifier le texte
APRS visible sur les anciens équipements ou sur APRS-IS.

Le mode texte RXT v2 est le choix prudent pour les déploiements actuels. Le
format natif apporte une réduction d’occupation radio et une association
encore plus précise, mais il reste expérimental. Les QRP Labs doivent être
testés en continu avant leur installation sur des sites inaccessibles.
