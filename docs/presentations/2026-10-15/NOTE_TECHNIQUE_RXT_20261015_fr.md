---
title: "LoRa APRS iGate — état technique du firmware RXT"
author: "Fabrice F4MLV"
date: "15 octobre 2026"
lang: fr-FR
geometry: margin=20mm
fontsize: 11pt
---

# Objet de cette note

Le firmware expérimental actuel réunit plusieurs évolutions liées mais
distinctes : RXT v2, les trames LoRa APRS natives, les statistiques d’activité
et la gestion distante sécurisée.

Cette note décrit ce qui est réellement présent dans le code, ce qui a été
validé sur matériel et les précautions à prendre avant d’installer des relais
sur des sites difficilement accessibles.

# 1. RXT v2

RXT mesure chaque liaison RF effectivement utilisée par un relais :

- le RSSI reçu ;
- le SNR ;
- le décalage de fréquence ;
- le délai entre la fin de la réception et la fin de la retransmission
  (*Time To Hop*, ou TTH).

Dans RXT v1, une mesure ne contenait pas l’identité du relais qui l’avait
produite. Le récepteur devait l’associer aux indicatifs du chemin à l’aide
d’une liste blanche. Une liste absente ou incomplète pouvait donc produire une
attribution incorrecte.

Dans RXT v2, chaque mesure transmise en mode texte contient une empreinte
calculée à partir de l’indicatif complet du relais. La liste blanche n’est plus
nécessaire. Si deux relais du même chemin possèdent la même empreinte et que
l’association ne peut pas être établie avec certitude, le firmware signale
l’ambiguïté au lieu d’attribuer arbitrairement la mesure.

## Compatibilité avec les firmwares existants

En mode texte, les données RXT sont placées dans un bloc binaire caché après
un octet nul. La trame APRS visible reste inchangée.

Un firmware CA2RXU classique s’arrête à cet octet nul et ne voit que la trame
APRS normale. Il peut donc continuer à la traiter sans ajouter RXT au
commentaire de l’utilisateur. Un relais ancien peut néanmoins perdre le bloc
caché lors de sa retransmission.

RXT est retiré avant l’envoi vers APRS-IS, MQTT, la WebUI ou un client TNC. Le
commentaire d’origine reste intact. RXT v1 et RXT v2 ne sont pas compatibles :
les relais participant à RXT dans un même réseau doivent être mis à jour de
manière coordonnée.

# 2. Trames LoRa APRS natives

Le firmware peut aussi transmettre une représentation binaire expérimentale
de la trame APRS. Après décodage, elle restitue la trame TNC2 d’origine octet
pour octet. Si cette restitution exacte est impossible, le firmware revient
automatiquement au format texte.

Ce format réduit le nombre d’octets transmis et donc le temps d’occupation du
canal. Sur les corpus utilisés pendant le développement, la réduction moyenne
mesurée par calcul est d’environ 22 à 25 %, selon le profil LoRa.

Dans une trame native, chaque mesure RXT est directement associée à une
position du chemin. Elle ne dépend donc pas d’une empreinte d’indicatif.

Trois modes de transmission sont disponibles :

- **texte** : mode par défaut et le plus compatible ;
- **natif** : temps d’occupation réduit et association RXT exacte ;
- **texte puis natif** : mode de transition destiné aux essais, mais chaque
  paquet est transmis deux fois.

# 3. Correctif validé le 13 octobre 2026

L’ancienne implémentation convertissait trop tôt les mesures natives vers la
représentation RXT textuelle. Cette conversion pouvait perdre l’identité
exacte d’une mesure lorsque le chemin était réécrit ou lorsque deux indicatifs
partageaient la même empreinte.

Le correctif conserve désormais, pour chaque paquet reçu, un contexte
indépendant comprenant la trame APRS, les mesures RF et l’identité exacte de
leurs relais. Ce contexte suit le paquet jusque dans la file de
retransmission.

Lorsqu’un alias consommé est retiré du chemin, les mesures sont repositionnées
à partir de l’indicatif complet du relais, et non de son empreinte.

## Essais réalisés

Les essais RF ont utilisé deux TTGO LoRa32 v2.1 proches. Un paquet construit
contenait `F4MLV-18` et `F6DEV-10`, deux indicatifs possédant la même empreinte
`C`. Seul `F6DEV-10` portait une mesure.

Après suppression d’un alias consommé, la mesure est restée attachée à
`F6DEV-10` dans la trame native retransmise.

Un paquet a ensuite été conservé sept secondes dans la file de retransmission
pendant qu’un second paquet était reçu. Les deux paquets ont conservé leurs
propres mesures. Une nouvelle réception n’écrase donc plus le contexte d’un
paquet déjà en attente.

Le commentaire utilisateur, y compris un texte ressemblant à une terminaison
RXT, est resté intact sur RF et sur APRS-IS. Le format radio natif n’a pas été
modifié par ce correctif.

# 4. Télémétrie d’activité

Les trois nouvelles valeurs sont des taux exprimés en paquets par heure :

| Paramètre | Signification |
|---|---|
| `RX_rate` | Paquets LoRa APRS valides acceptés après filtrage de la liste noire |
| `RelRate` | Paquets acceptés et placés dans la file de retransmission |
| `DrpRate` | Paquets refusés : liste noire, doublon, chemin, retour vers soi-même ou `NOGATE` |

Ces compteurs sont indépendants. L’égalité suivante n’est donc pas attendue :

```text
RX_rate = RelRate + DrpRate
```

Un iGate peut accepter des paquets sans les retransmettre. Inversement, un
paquet placé sur liste noire augmente `DrpRate` sans augmenter `RX_rate`.

Les compteurs sont ramenés à une heure selon la durée réelle écoulée depuis le
rapport précédent. Après un redémarrage, une première fenêtre courte peut donc
produire temporairement une valeur élevée. Par exemple, des paquets comptés
pendant quinze minutes sont multipliés par quatre pour obtenir un taux horaire.

Les messages `EQNS` ne multiplient pas ces valeurs : les coefficients des
trois canaux sont `0,1,0`. Le pic observé après le remplacement de SOMTNP peut
donc provenir d’une courte fenêtre de mesure après redémarrage, d’une hausse
réelle du trafic, ou des deux. Les heures exactes des rapports sont nécessaires
pour les départager.

# 5. Déploiement sur les QRP Labs

Les variantes TTGO, Heltec et QRP Labs ont été compilées avec ces fonctions.
Le correctif du 13 octobre a cependant été validé en radio uniquement entre
deux TTGO.

Une compilation réussie ne démontre pas la stabilité à long terme sur le QRP
Labs. L’ESP32-S3, l’alimentation, le pilote radio, la mémoire, les files
d’attente, les reconnexions et le watchdog peuvent se comporter différemment.

Les futurs relais étant destinés à des sommets potentiellement inaccessibles
pendant plusieurs mois, chaque QRP Labs devrait fonctionner plusieurs jours
sur banc avec sa configuration définitive et une charge radio réaliste. Il
faut notamment surveiller :

- les redémarrages et leur cause ;
- l’évolution de la mémoire disponible ;
- la réception, l’attribution et la retransmission RXT ;
- les files de retransmission ;
- les reconnexions réseau et APRS-IS ;
- les commandes distantes.

Le redémarrage d’un Heltec toutes les dix à quinze minutes n’est pas encore
expliqué. La cause de reset et le journal série doivent être récupérés avant
de conclure à un défaut matériel ou à une régression du firmware.

# 6. Gestion distante sécurisée

Cette fonction est indépendante de RXT. Techniquement, il ne s’agit pas de
TOTP.

Chaque équipement utilise une clé secrète aléatoire propre au couple formé par
l’équipement et son contrôleur. L’identité du contrôleur, la cible, la
commande et un compteur persistant sont protégés par HMAC-SHA-256. Une fenêtre
anti-rejeu empêche qu’une commande déjà reçue soit exécutée une seconde fois,
y compris après un redémarrage ou lorsque plusieurs copies arrivent par des
chemins RF différents.

Une fois une clé installée, les formes non signées de `?EM=ON/OFF`,
`?TX=ON/OFF` et `?COMMIT` sont refusées. Sans clé, le fonctionnement historique
fondé sur la liste des gestionnaires reste actif.

Une copie de chaque clé doit être conservée hors de l’équipement, sur papier
ou dans un gestionnaire de mots de passe sécurisé. Graywolf doit utiliser
l’identité de contrôleur exacte configurée dans le nœud concerné.

# Conclusion

RXT v2 supprime la dépendance à une liste blanche et protège mieux le contenu
APRS. Le format natif réduit le temps d’occupation radio et conserve une
association exacte entre les mesures et les relais. Le correctif du 13
octobre a validé cette association, la réécriture du chemin et l’isolation des
mesures dans la file d’attente sur deux TTGO.

Ces résultats sont suffisants pour poursuivre les essais, mais pas pour
garantir plusieurs mois de fonctionnement sans accès physique sur du matériel
QRP Labs. Un essai continu sur les unités et configurations destinées aux
sommets reste donc nécessaire avant leur déploiement.
