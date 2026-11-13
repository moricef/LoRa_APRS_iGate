# Collisions entre relais voisins — essais du 27 octobre 2026

> Compte rendu historique. Les conclusions portent sur les versions et les conditions décrites dans ce document. Pour les références actuelles, consulter l’[index documentaire](../../README.md).

Ce document archive un sujet ouvert pendant les essais du 27 octobre 2026 et
mis de côté volontairement. Il n'appartient pas au plan en cours (format
commun natif et PPM). Le code essayé est conservé sur la branche locale
`archive/relay-jitter-20261027`.

## Point de départ

Pendant l'essai PPM du 26 octobre, F4MLV-10 a perdu une trame parce que
l'écoute du canal (CAD) lancée avant sa propre émission avait coupé une
réception en cours. Cette correction est commitée sur `rxt` (commit
« Do not let a CAD scan cut a reception in progress ») : la file d'émission
attend la fin d'une réception en cours, et une écoute qui trouve le canal
occupé rend la main au lieu de sonder en boucle, la radio repassant en
réception. Aucun cas où une réception était en cours au moment d'émettre n'a
encore été observé sur l'air ; la correction n'est donc vérifiée qu'à la
compilation et par le fonctionnement normal des cartes.

## Ce qui a été observé

Stations : F4MLV-2, F4MLV-10 et F4MLV-15 (GENATX), regroupées sur le même
site, toutes en digi mode 2 pendant l'essai, et le T-Deck F4MLV-7 émettant
des balises Mic-E avec `WIDE1-1,WIDE2-1`. Journaux : `logs/cad-20261027/`
(séries de F4MLV-2 et F4MLV-15 horodatées par le PC à la seconde, SD de
F4MLV-10 en millisecondes depuis le démarrage ; les sauvegardes de
configuration contiennent des secrets).

Avec le comportement d'origine (seulement la correction du CAD sur F4MLV-2 et
F4MLV-10), de 13:34 à 13:50 heure locale, F4MLV-2 et GENATX ont terminé leur
émission dans la même seconde pour huit trames relayées, dont la balise du
T-Deck de 13:35:41, et aucune carte n'a jamais signalé de canal occupé. Les
relais partent environ une demi-seconde après la fin de la trame reçue ;
l'écart tiré par le CAD d'origine (deux tranches libres puis une à quatre
tranches au hasard) est de l'ordre de quelques centaines de millisecondes.

Essai d'une attente aléatoire de 0 à 2 s avant chaque relais, avec
annulation d'un relais en file quand la même trame (source, destinataire,
contenu, sans chemin ni RXT) est entendue répétée par une autre station :

- de 15:38 à 15:42, GENATX n'ayant pas encore l'attente, F4MLV-2 a annulé
  trois relais (dont les balises du T-Deck de 15:38:51 et 15:40:53) après
  avoir entendu GENATX ; deux trames ont encore été émises par les deux
  cartes dans la même seconde (15:41:24 et 15:41:52) ;
- de 15:52 à 15:54, les trois cartes ayant l'attente, la balise de 15:52:42,
  reçue directement par les trois, a été relayée par les trois, sans
  annulation ; pour les trois balises suivantes, seul F4MLV-2 a reçu la
  trame d'origine, GENATX et F4MLV-10 enregistrant une erreur CRC à signal
  fort (−49 à −103 dBm), puis tous deux ont relayé la copie de F4MLV-2 au
  second saut.

## Limites de ces observations

Deux émissions terminées dans la même seconde ne prouvent pas une collision :
il faudrait les instants de début et de fin avec une précision suffisante.
Une erreur CRC, même à signal fort, n'en établit pas la cause. L'affirmation
selon laquelle les départs simultanés disparaîtraient une fois toutes les
stations équipées de l'attente est fausse : des tirages proches restent
possibles, et l'essai de 15:52 le montre.

L'annulation réduit la couverture quand deux relais qui s'entendent
desservent des zones différentes. Sur nos cartes regroupées, elle paraît
utile ; ce résultat ne se généralise pas au réseau sans règle tenant compte
du chemin.

## Pour reprendre plus tard

Définir d'abord le comportement recherché, notamment la couverture à
préserver, avant tout mécanisme. Instrumenter ensuite le firmware sans
changer son comportement : attente tirée, début et fin exacts de chaque
émission, reports pour réception en cours ou canal occupé, annulations ;
horodater les journaux série à la milliseconde. Comparer alors des séries de
balises avec différentes attentes (0–2 s, 0–5 s) en comptant les émissions,
les réceptions réussies sur chaque carte et le retard ajouté, et vérifier
l'interaction entre le CAD et la détection d'une réception en cours.

Les erreurs CRC sur les trames d'origine du T-Deck sont une question
distincte : croiser l'heure des émissions du T-Deck (son moniteur série) avec
les erreurs CRC horodatées des cartes.

Les collisions entre relais voisins concernent tout le réseau et le firmware
de Ricardo ; elles relèvent d'une discussion en amont, pas d'un correctif
local.
