# Routage retour des messages APRS

L'iGate n'apprend un chemin de retour qu'à partir d'un paquet qu'il a
effectivement reçu par LoRa. Lorsqu'APRS-IS fournit ensuite un message destiné
à cette station entendue récemment, l'iGate l'encapsule dans une trame APRS
tierce et utilise les relais appris dans l'ordre inverse.

Par exemple, un paquet reçu sous la forme :

```text
N7UV-4>APLRT1,N7UV-6,SOMTNP*,WIDE2-1:...
```

enseigne le chemin de retour RF suivant :

```text
SOMTNP,N7UV-6
```

Les formes `N7UV-6*,SOMTNP*` et la forme canonique
`N7UV-6,SOMTNP*` sont toutes deux acceptées. Le chemin utilisé est la portion
complète qui se termine par le dernier `*` ; les alias non consommés et les
éléments propres à Internet ne sont pas copiés dans le chemin de retour.

## Politique de sélection du chemin

La première copie d'un paquet logique reçue pendant la fenêtre de
déduplication de 25 secondes est retenue. Les copies ultérieures qui possèdent
la même source et le même champ d'information ne remplacent pas son chemin,
même si elles sont arrivées par d'autres digipeaters. Il s'agit d'une
préférence délibérée pour le chemin qui a livré le paquet en premier, et non de
l'affirmation qu'il est toujours le plus puissant ou le plus fiable.

Une première copie reçue directement enregistre donc `DIRECT`, même si une
copie relayée arrive ensuite. Un paquet ultérieur comportant une information
différente peut enseigner un nouveau chemin.

## Sécurité et durée de validité

- Un chemin appris expire avec l'entrée `rememberStationTime` existante.
- Après un redémarrage, une station doit être entendue de nouveau avant que des
  messages provenant d'APRS-IS lui soient transmis par RF.
- Il n'existe aucun repli sur `Config.beacon.path`. Si aucun chemin valide
  n'est connu, le message n'est pas transmis.
- Le mécanisme s'applique uniquement aux messages APRS-IS destinés aux
  stations entendues récemment ; il ne modifie ni les chemins des balises, ni
  le routage WIDE générique.
- Les modes de digipeater fill-in, régional et à chemin explicite acceptent
  tous leur propre indicatif comme prochain élément non utilisé d'un chemin
  source appris.

Les diagnostics série utilisent le préfixe `[RETURN-PATH]` pour les chemins
appris, sélectionnés et rejetés.

## Validation RF — 27 septembre 2026

Le firmware de test a été installé sur F4MLV-15 (iGate) et F4MLV-2
(digipeater). F4MLV-15 utilisait `messagesToRF=true` et le filtre
`m/10 g/F4MLV-MC`. L'enregistrement de la configuration ayant redémarré
l'iGate, le chemin a été appris de nouveau après la dernière modification de
configuration.

L'iGate a transmis un paquet RF de test sous l'identité simulée F4MLV-MC, puis
a reçu sa répétition RF effective par F4MLV-2 :

```text
F4MLV-MC>APLRG1,F4MLV-2*:>Reverse route relearn 20260927-8
[RETURN-PATH] Learned F4MLV-MC via F4MLV-2
```

Une connexion de test APRS-IS distincte et authentifiée sous l'indicatif
F4MLV-14 a envoyé :

```text
F4MLV-14>APLRG1,TCPIP*::F4MLV-MC :Test retour complet 20260927-9
```

La capture série de l'iGate a confirmé la réception depuis APRS-IS, la
sélection du chemin appris et la transmission :

```text
Rx Message (APRS-IS): F4MLV-14>APLRG1,TCPIP*,qAC,T2SYDNEY::F4MLV-MC :Test retour complet 20260927-9
[RETURN-PATH] Message to F4MLV-MC via F4MLV-2
---> LoRa Packet Tx : F4MLV-15>APLRG1,F4MLV-2:}F4MLV-14>APLRG1,TCPIP,F4MLV-15*::F4MLV-MC :Test retour complet 20260927-9
```

La capture RF a ensuite reçu la répétition effective :

```text
F4MLV-15>APLRG1,F4MLV-2*:}F4MLV-14>APLRG1,TCPIP,F4MLV-15*::F4MLV-MC :Test retour complet 20260927-9
LOCAL -- RSSI:-56 SNR:+9.00 FO:-2081
```

Cela valide l'apprentissage RF, la réception APRS-IS, la transmission sur le
chemin explicite appris et la répétition RF par le digipeater demandé.
F4MLV-MC était une source simulée et non un récepteur physique. À ce stade, la
livraison et l'ACK sur une véritable station terminale, ainsi que les chemins
de retour à plusieurs sauts sur du matériel réel, n'avaient pas encore été
testés. Les essais suivants couvrent séparément la livraison à un terminal
physique avec ACK et l'exécution d'un chemin à deux relais.

Utiliser des SSID numériques distincts, compris entre 0 et 15, pour l'émetteur
et la connexion de test APRS-IS. Ne pas réutiliser l'identifiant de connexion
de l'iGate actif pour une seconde connexion. L'émetteur APRS-IS doit également
être différent de chaque relais du chemin appris.

### Livraison physique isolée par un relais

Les profils radio ont été délibérément séparés afin que la liaison descendante
ne puisse pas atteindre F4MLV-7 directement :

- F4MLV-15 émettait avec le profil EU (433,775 MHz, SF12/CR5) et recevait avec
  le profil Pologne (434,855 MHz, SF9/CR7).
- F4MLV-2 recevait avec le profil EU et répétait avec le profil Pologne.
- F4MLV-7 utilisait le profil Pologne pour la réception comme pour l'émission.

Après qu'un paquet RF synthétique a enseigné à F4MLV-15 que F4MLV-7 était
joignable par F4MLV-2, F4MLV-14 a envoyé le message numéroté `P201` par
APRS-IS. F4MLV-2 a reçu l'émission EU de l'iGate et l'a répétée avec le profil
Pologne. F4MLV-7 a reçu le message et produit l'ACK ; puisque l'iGate écoutait
avec le profil Pologne, il a reçu cet ACK directement et l'a envoyé à
APRS-IS :

```text
F4MLV-7>APLRT1,WIDE1-1,WIDE2-1,qAR,F4MLV-15::F4MLV-14 :ackP201
```

L'incompatibilité des profils de F4MLV-15 et de F4MLV-7 rend impossible une
liaison descendante directe dans cette configuration. Cet essai valide donc la
livraison physique du message par le chemin appris à un relais, la production
de l'ACK par le destinataire et le retour de cet ACK vers APRS-IS.

### Chemin isolé à deux relais

Un second essai isolé a exercé la consommation d'un chemin explicite à deux
éléments. F4MLV-15 émettait de nouveau avec le profil EU et recevait avec le
profil Pologne ; F4MLV-2 recevait en EU et émettait en Pologne. F4MLV-7 a été
temporairement configuré comme digipeater sur le profil Pologne, acceptant son
propre indicatif comme prochain élément explicite du chemin.

Un paquet source RF synthétique pour F4MLV-MC portait le chemin utilisé
`F4MLV-7,F4MLV-2*`. F4MLV-15 a par conséquent appris le chemin de retour
`F4MLV-2,F4MLV-7`. F4MLV-14 a ensuite envoyé le message APRS-IS `P303`. La
capture RF finale, après consommation par les deux relais de leurs éléments de
chemin respectifs, était :

```text
F4MLV-15>APLRG1,F4MLV-2*,F4MLV-7*:}F4MLV-14>APLRG1,TCPIP,F4MLV-15*::F4MLV-MC :Test deux relais isoles{P303
```

La séparation de la fréquence et de la modulation imposait le premier saut par
F4MLV-2, tandis que la capture finale, avec les deux indicatifs explicites
marqués comme utilisés, prouve que F4MLV-2 puis F4MLV-7 ont traité le chemin
dans l'ordre attendu. F4MLV-MC était synthétique lors de cet essai ; `P303`
valide donc l'exécution du chemin à deux relais, et non la livraison ou l'ACK
par un troisième terminal physique. La livraison au terminal physique et son
ACK avaient déjà été établis séparément avec `P201`.

### Limite restante au niveau du réseau

Dans la configuration testée, APRS-IS a livré la même liaison descendante à
plusieurs iGates éligibles, qui l'ont transmise indépendamment par RF. Aucune
sélection exclusive d'une passerelle n'a été observée. La gestion des doublons
par APRS-IS ne doit donc pas être assimilée à une coordination entre les
iGates émetteurs.

Le chemin de retour appris traite l'asymétrie du chemin RF pour un iGate pris
individuellement dans les cas RF à un et deux relais exercés ici. La dérivation
du chemin direct et la construction de la trame tierce sont couvertes par les
tests sur hôte. Le mécanisme ne coordonne pas les passerelles lorsque plusieurs
iGates couvrent le même destinataire. Ce problème distinct, situé au niveau du
réseau, reste à caractériser ou à atténuer.
