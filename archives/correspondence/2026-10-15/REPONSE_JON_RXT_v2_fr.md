---
title: "RXT v2 sur le réseau LoRa APRS"
date: "15 octobre 2026 — brouillon pour relecture"
lang: fr-FR
---

Jon,

RXT permet de connaître la qualité des liaisons radio successives d’un paquet,
au lieu de connaître seulement celle du dernier signal reçu par l’iGate. La
version actuelle conserve le principe de ton travail initial et améliore
l’identification des relais, le transport des mesures et leur restitution aux
applications.

# Mesures reçues depuis SOMTNP

Prenons ce chemin :

**Tracker → SOMTNP → autre relais → iGate → Graywolf**

SOMTNP mesure le signal reçu du tracker. Lorsqu’il répète le paquet, il y joint
son RSSI, son SNR, le décalage de fréquence et le temps écoulé entre la fin de
la réception et la fin de sa retransmission, appelé TTH.

Le relais suivant fait la même chose pour le signal reçu de SOMTNP. L’iGate
dispose alors des mesures transportées par les relais, auxquelles s’ajoutent
ses propres mesures de réception du dernier relais. Graywolf peut présenter
ces observations liaison par liaison.

Ainsi, une bonne réception de SOMTNP chez toi ne signifie pas nécessairement
que SOMTNP entend bien les trackers. RXT permet de distinguer ces deux
liaisons. Pour un réseau de montagne, cela aide à repérer un tronçon faible,
un décalage de fréquence ou une attente importante avant retransmission,
sans accéder physiquement au relais.

# Identification des relais en RXT v2

En v1, les quatre caractères d’une mesure ne disaient pas quel relais l’avait
produite. Le récepteur devait retrouver cette association à partir du chemin
et d’une liste blanche de relais RXT. Une liste incomplète pouvait conduire
à une attribution incorrecte.

En v2, un cinquième caractère contient une empreinte de l’indicatif du relais,
SSID compris. Le récepteur rapproche cette empreinte des indicatifs
effectivement utilisés dans le chemin. La liste blanche n’est plus nécessaire.

Cette empreinte est courte : deux indicatifs peuvent partager la même valeur.
L’ordre des mesures et du chemin permet souvent de les départager. Lorsque
cela reste ambigu, le logiciel le signale au lieu d’attribuer une mesure au
hasard. Un relais présent dans le chemin mais n’ayant fourni aucune mesure
apparaît sans données ; cela ne signifie pas que sa liaison était mauvaise.

Le firmware transporte au maximum trois mesures de relais. Il n’en ajoute
qu’aux paquets réellement reçus par radio puis répétés. Ses propres balises
et réponses ne reçoivent pas artificiellement les mesures du dernier paquet
entendu. Les messages APRS, ACK et REJ sont également exclus de l’ajout RXT.

# Transport radio et sortie vers APRS-IS

En émission texte, réglage par défaut, la trame APRS reste intacte. Les mesures
RXT sont transportées séparément après un octet nul, dans le même paquet radio.

Les firmwares CA2RXU qui lisent la réception comme une chaîne de caractères
s’arrêtent à cet octet : ils voient la trame APRS habituelle. Ils peuvent la
traiter, mais leur retransmission ne conserve pas le bloc RXT caché. La
compatibilité APRS est donc conservée, sans garantir la continuité des mesures
à travers un ancien relais.

L’iGate transmet à APRS-IS la trame APRS débarrassée de RXT. Les mesures restent
disponibles localement dans la WebUI et sur les interfaces destinées aux
applications, notamment les sorties TNC2 et JSON utilisées pour les afficher.
En mode KISS, aucune ligne de mesures textuelles n’est insérée dans le flux
binaire.

RXT v1 et v2 utilisent des formats différents. Pour obtenir une chaîne de
mesures cohérente, il faut mettre à jour les équipements RXT concernés,
y compris ceux qui décodent les mesures.

# Format natif et correctif du 13 octobre

Le firmware contient aussi un format radio natif binaire expérimental,
distinct de RXT v2. Il réduit la taille des trames et associe les mesures
directement aux positions du chemin. Il n’est pas nécessaire de l’activer
pour bénéficier de RXT v2 en mode texte.

Le correctif du 13 octobre préserve cette association native pendant la
réécriture du chemin et l’attente en file. Les essais sur deux TTGO ont
confirmé ces points, notamment avec deux indicatifs ayant la même empreinte
et avec un paquet retenu sept secondes pendant la réception d’un autre.

# Compteurs RX, Relay et Drop

Ils décrivent l’activité du nœud qui les émet, indépendamment des mesures RXT :

- **RX_rate** : réceptions LoRa APRS acceptées après filtrage de la liste noire.
- **RelRate** : paquets acceptés et placés dans la file de répétition.
- **DrpRate** : rejets par les règles de traitement : doublons, chemin,
  retour vers soi-même, liste noire, NOGATE, notamment.

RelRate ne confirme ni une émission réussie ni la réception par le destinataire.
DrpRate n’est pas un taux de pertes radio : un doublon correctement écarté
l’augmente aussi. Les trames rejetées pour erreur CRC ne sont pas comptées
comme des réceptions acceptées.

Ces trois valeurs ne forment pas un bilan comptable. En particulier,
RX_rate = RelRate + DrpRate n’est pas une égalité attendue.

À chaque rapport, les compteurs sont divisés par la durée réelle écoulée,
puis exprimés en paquets par heure. Ils sont ensuite remis à zéro. Les
coefficients EQNS actuels des canaux d’activité sont 0,1,0 : la valeur
transmise est déjà le taux horaire.

Je ne peux pas attribuer ton pic ×4 à une cause précise sans examiner les
trames. Une fenêtre de quinze minutes ne crée pas, à elle seule, un facteur
quatre artificiel : la normalisation doit justement compenser sa durée.
Une ancienne définition des canaux encore utilisée par le logiciel
d’affichage, un changement de firmware/configuration ou une activité
réellement différente sont des pistes à vérifier. Il faut comparer les
rapports bruts avant et après remplacement, leurs horaires et les messages
PARM, UNIT et EQNS reçus pour SOMTNP.

# Mise à jour d’OATMNN et TLGRPH

La version publiée le 13 octobre porte toujours le numéro 4.0.2RXT ; la date
de compilation est donc nécessaire pour identifier précisément le binaire.
Pour conserver la compatibilité avec les équipements APRS existants, je
garderais l’émission en mode texte et vérifierais que les récepteurs décodent
bien RXT v2. Il n’y a plus de liste blanche RXT à entretenir.

Avant installation, le contrôle utile est un paquet réellement reçu puis
répété par chaque unité : indicatif du relais correctement reconnu, mesures
plausibles, commentaire inchangé et copie propre sur APRS-IS. Il faut aussi
vérifier la stabilité de chaque carte avec son alimentation et ses réglages
définitifs. Les essais du correctif natif sur deux TTGO ne constituent pas
une validation longue durée des variantes Heltec ou QRP Labs.

L’avancée pour les utilisateurs est concrète : nous pouvons observer les
liaisons intermédiaires d’un réseau LoRa APRS, identifier les relais ayant
effectué chaque mesure et conserver une trame APRS normale pour les autres
usages. Cela donne des éléments pour diagnostiquer le réseau ; cela ne
constitue pas encore une mesure du taux de livraison de bout en bout.
