# Format commun natif et PPM — variantes chiffrées

Document de travail du 27 octobre 2026, local, non publié. Il prépare le
choix d'un format commun à proposer ensemble à Walter K6ATV et Jon N7UV,
puis, pour la variante retenue, à Bernard F6BVP (réglementation) et à
F4HOF (cadrage sur l'air), avant toute proposition au firmware de Ricardo.

## Objectif

Un seul format de trame qui réduise le temps d'antenne, qu'un relais sache
traiter (décompresser, modifier le chemin, ajouter éventuellement une mesure
RXT, recompresser) et qui respecte la réglementation sur l'identification
des stations. Le natif
structure la trame et rattache les mesures RXT aux relais ; le PPM de Walter
compresse une suite d'octets de façon réversible. Les deux ne s'opposent
pas : la question est ce que l'un structure et ce que l'autre compresse.

## Réglementation

La décision ARCEP n° 2012-1241 interdit, à l'article 1, « de coder les
transmissions entre des stations d'amateur pour en obscurcir le sens », et
impose, à l'article 4, aux stations d'amateur de « transmettre leur
indicatif d'appel » à de courts intervalles, au moins au début et à la fin de
toute période d'émission. Elle n'exige ni l'ASCII ni une lecture sans
décompression. Un codage publié et réversible, dont un décodeur public
restitue l'indicatif, n'obscurcit pas le sens ; la simple présence d'une
compression n'établit pas une infraction.

L'indicatif écrit en clair n'est donc pas un critère éliminatoire. C'est une
option technique, qui permet à une station sans décodeur de savoir qui émet,
et que ce document chiffre à ce titre.

L'article 4 porte sur la station qui émet et fixe l'identification par
période d'émission, pas par trame. Pour une trame relayée, la station qui
émet est le relais. Notre règle de conception, qui va au-delà de l'obligation
réglementaire, est que chaque trame émise par un relais porte son indicatif :
dans toutes les variantes, il l'inscrit dans le chemin ; la question est
seulement sous quelle forme. L'avis de Bernard F6BVP, qui proposait une
forme réversible et publiée, sera sollicité sur la variante retenue, en
s'appuyant sur ce texte.

## Tailles de trame

Toutes les mesures portent sur les mêmes captures que les études du
25 octobre : 26 303 réceptions en Ariège et 990 paquets du réseau de Jon en
Arizona. Les tailles comprennent le préfixe radio, sans bloc RXT. Chaque
trame compressée a été décodée et comparée à l'original. Calculs sur PC
(`logs/format-variants-20261027/`).

| Variante | Source en clair | Ariège, gain en octets | Ariège, temps d'antenne SF12 | Arizona, gain en octets | Arizona, temps d'antenne SF7 |
|---|---|---:|---:|---:|---:|
| Natif, texte libre au mieux (Huffman ou PPM) | non | 41,5 % | 31,9 % | 46,0 % | 40,4 % |
| Natif, source en texte brut | oui | 36,8 % | 28,3 % | 44,0 % | 38,6 % |
| PPM du paquet entier | non | 47,7 % | 36,5 % | 50,0 % | 44,0 % |
| Source en clair, puis PPM du reste | oui | 42,0 % | 32,3 % | 47,7 % | 41,9 % |
| Source en clair, puis PPM du paquet entier | oui | 36,1 % | 27,7 % | 43,3 % | 38,1 % |

Les gains sont mesurés par rapport au texte actuel (72,5 octets en moyenne
en Ariège, 109,9 en Arizona).

Écrire la source en clair coûte de 2 à 6 points de gain selon le corpus et
le format. Parmi les variantes qui le font, la source en clair suivie du PPM
du reste du paquet est la plus compacte, à peu près au niveau du natif
actuel. Le natif
avec source en clair vient ensuite, avec 5 points de moins en Ariège et
4 en Arizona. Répéter la source en clair devant le PPM du paquet entier est
la moins bonne solution.

Ces chiffres ne tranchent pas à eux seuls. Le natif porte des fonctions
absentes du PPM : champs structurés et rattachement de chaque mesure RXT au
relais qui l'a faite. Ce document ne mesure pas encore ce que coûtent ces
fonctions dans chaque variante.

## Coût du RXT

Dans le natif, les mesures RXT forment un bloc d'un octet de masque, qui
indique la position de chaque relais mesuré dans le chemin, suivi de
4 octets par mesure. Ce bloc ne dépend pas du codage du reste de la trame :
il peut précéder tel quel une charge PPM, le relais concerné étant retrouvé
par sa position dans le chemin décompressé. Le format texte actuel coûte
2 octets plus 5 par mesure. Toutes les variantes compressées sont donc
comptées ici avec le bloc natif, et le texte avec son bloc actuel.

En Ariège, le nombre de mesures vient du journal SD (1 023 paquets avec une
mesure, 14 avec deux, sur 26 303). Le relevé d'Arizona ne contient pas ce
nombre : on y suppose une mesure pour chaque paquet relayé au moins une fois
(859 sur 990). C'est un scénario, pas une majoration garantie, puisqu'un
paquet peut porter jusqu'à trois mesures.

Ces tailles avec RXT sont calculées en ajoutant le coût du bloc à celles du
tableau précédent ; seules les trames sans RXT ont été encodées et
restituées.

| Variante | Source en clair | Ariège, gain en octets | Ariège, SF12 | Arizona, gain en octets | Arizona, SF7 |
|---|---|---:|---:|---:|---:|
| Natif, texte libre au mieux | non | 41,4 % | 31,8 % | 45,1 % | 39,9 % |
| Natif, source en texte brut | oui | 36,7 % | 28,3 % | 43,2 % | 38,3 % |
| PPM du paquet entier | non | 47,6 % | 36,4 % | 48,9 % | 43,2 % |
| Source en clair, puis PPM du reste | oui | 42,0 % | 32,3 % | 46,7 % | 41,3 % |
| Source en clair, puis PPM du paquet entier | oui | 36,0 % | 27,7 % | 42,6 % | 37,6 % |

Le RXT retire au plus environ un point de gain et ne change pas le classement : avec
un bloc commun, il ne départage pas les variantes. Ce qui les départage est
le traitement par un relais.

## Temps de traitement sur ESP32

Mesuré le 27 octobre sur un ESP32 classique à 240 MHz (TTGO LoRa32 V2.1,
F4MLV-15), avec les mêmes codecs, sur 400 paquets de l'Ariège et 200 de
l'Arizona tirés au hasard. Un relais décode la trame reçue puis encode celle
qu'il émet ; les deux opérations sont chronométrées ensemble pour chaque
paquet, sans modification du chemin. Valeurs en millisecondes. Les
600 paquets ont été restitués et réencodés à l'identique.

| Codage | Ariège, moyenne | Ariège, 95e centile | Ariège, maximum | Arizona, moyenne | Arizona, 95e centile | Arizona, maximum |
|---|---:|---:|---:|---:|---:|---:|
| Natif, texte en Huffman | 7,9 | 17,7 | 40,6 | 19,5 | 32,7 | 40,8 |
| Natif, texte au mieux (Huffman ou PPM) | 19,1 | 43,8 | 116,7 | 51,9 | 74,9 | 117,6 |
| PPM du paquet entier | 39,6 | 59,4 | 89,1 | 61,2 | 79,5 | 94,7 |
| Source en clair, puis PPM du reste | 34,5 | 54,1 | 84,3 | 56,8 | 74,1 | 90,8 |

Les paquets que le PPM refuse de compresser (2 sur 400 en Ariège pour le
paquet entier, 1 pour le reste) partent en texte et ne sont pas chronométrés.

En SF12, où une trame compressée dure environ 2 s, le PPM ajoute en moyenne
environ 2 % au délai de relais, et environ 5 % dans le pire cas observé ; le
natif au mieux atteint environ 6 % dans son pire cas. Ce calcul n'occupe pas
le canal. En SF7, où une trame dure environ 130 ms, le PPM ajoute en moyenne
un délai d'environ la moitié de la durée de la trame (61 ms), jusqu'à
95 ms ; cela reste un retard de relais, pas une occupation du canal, mais le
natif en Huffman y est trois fois plus rapide en moyenne.

Côté mémoire, le décodeur PPM ajoute environ 127 Kio de flash au firmware de
l'iGate. Dans le firmware de mesure, toute la pile utilisée, codecs compris,
est restée sous 3,3 Kio sur les 8 Kio de la tâche principale. La mémoire de
travail des codecs n'a pas été isolée de celle du programme de mesure.

## Mesures restantes

La mémoire de travail des codecs sur la carte, isolée du reste, et le
traitement complet dans le firmware réel (modification du chemin et ajout du
RXT compris), et non dans un programme de mesure séparé. Les 127 Kio de flash
viennent de la comparaison des builds du 27 octobre (1 763 585 octets pour le
build PPM de GENATX contre 1 633 921 pour celui de `rxt`).

L'indicatif du relais : si l'on retient l'option de l'indicatif en clair,
c'est celui de la station qui émet qui compte ; il faut alors mesurer une
variante où le relais écrit son propre indicatif en clair, et pas seulement
la source.

La robustesse : rejet d'une trame tronquée ou d'un type inconnu, et
version du modèle PPM, qui est figé et identifié dans le préfixe.

## Hors périmètre

Le mode PPM avec historique reste écarté tant que son contrôle de référence
n'est pas renforcé. Les collisions entre relais voisins sont traitées à part
(`docs/tests/2026-10-27/RELAY_COLLISIONS_20261027_fr.md`).
