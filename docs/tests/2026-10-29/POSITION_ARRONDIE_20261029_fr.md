# Position arrondie : gain marginal et différences de restitution

Essai sur PC du 29 octobre 2026. Comparaison de `PACKPOS` de Walter K6ATV
au bloc de position natif sans cet arrondi, en trames autonomes uniquement.
Aucun firmware flashé, aucune trame de test émise.

## Méthode

Code radiohound/aprs-compression-proposal, branche `hybrid`, commit
`d0aa966`, conservé sans modification. Un programme appelle ses fonctions
de coût et récupère le contenu de position qu'il reconstruit. Les coordonnées,
le cap et la vitesse de l'original et du contenu reconstruit sont ensuite
calculés indépendamment.

Jeux de test inchangés : 8 062 paquets de 44 stations des journaux SD de
F4MLV-10, et 224 paquets de 11 stations du relevé N7UV du 29 septembre.
Compartiments MD5 0 à 2 pour les tests, suppression des répétitions
consécutives identiques, tous destinataires conservés. Aucun relayage
supplémentaire n'est simulé et aucun coût RXT ajouté ; certains paquets
de l'entrée portent déjà des relais.

Les variantes B utilisent `NATIVE=1` et `PLAINTEXT=1`. Le modèle PPM publié
`model_hybridB_info.c` est maintenu identique pour toutes. Les modèles
entraînés sur les commentaires seuls utilisés par Walter n'étaient pas
publiés au moment de l'essai : ces tailles ne reproduisent donc pas sa
mesure de 47,2 %. Les différences de position ne dépendent pas du modèle.

La liste de destinations est produite par son outil sur les stations
d'entraînement des deux captures réunies : 47 destinations exactes et
27 motifs aprsorg/aprs-deviceid. Cette liste est locale et expérimentale,
pas une table commune de protocole. Les stations de test en sont exclues.
Le coût Mic-E de quatre octets reste une estimation non vérifiée par un
encodeur de destination effectif.

## Tailles calculées

Moyennes sur tous les paquets, préfixe radio compris. La position « sans
arrondi » est la référence `POSNATIVE` du banc.

| Variante | Ariège, octets | Arizona, octets |
| --- | ---: | ---: |
| B, information en PPM | 51,04 | 59,40 |
| B + table de destinations | 47,39 | 55,84 |
| B + table de destinations + position native sans arrondi | 46,32 | 55,53 |
| B + table de destinations + position arrondie | 45,49 | 54,64 |

La table de destinations économise 3,65 octets par paquet en Ariège et
3,56 en Arizona. Ses entrées exactes couvrent 92,4 % et 91,1 % des paquets.
Le traitement estimé Mic-E concerne respectivement 115 et un paquet ; les
autres destinations utilisent l'échappement.

**L'arrondi seul économise 0,84 octet par paquet en Ariège et 0,89 en
Arizona par rapport à la position native sans arrondi.** Aucune taille
calculée ne dépasse les 255 octets de charge utile LoRa sur ces données.

## Gain marginal de temps d'antenne

Calcul LoRa : BW125, préambule de huit symboles, en-tête explicite et CRC.
Les deux dernières variantes sont comparées : les effets de la table de
destinations et de la structuration des positions sont exclus de ce gain.

| Jeu et profil | Sans arrondi | Avec arrondi | Économie moyenne | Réduction |
| --- | ---: | ---: | ---: | ---: |
| Ariège, SF12 CR 4/5 | 2 245,09 ms | 2 218,10 ms | 26,99 ms | 1,20 % |
| Arizona, SF7 CR 4/6 | 124,50 ms | 122,72 ms | 1,78 ms | 1,43 % |

La durée baisse pour 1 328 des 8 062 paquets ariégeois (16,5 %) et 65 des
224 paquets arizoniens (29,0 %). Les autres restent dans le même bloc de
symboles LoRa. Ces durées sont calculées, pas mesurées sur l'air ; elles
dépendent des longueurs produites avec le modèle PPM fixé pour cet essai.

## Différences de restitution

L'analyse porte sur les positions reconnues par le parseur de Walter.
Les autres contenus restent inchangés. Le déplacement est la distance
horizontale entre la position APRS d'origine et celle du texte reconstruit,
sur une sphère de rayon 6 371 008,8 m. Ce n'est pas une mesure par rapport
à une position physique connue.

| Observation | Ariège | Arizona |
| --- | ---: | ---: |
| Positions reconnues | 7 157 | 207 |
| Contenu reconstruit différent | 6 617 (92,45 %) | 200 (96,62 %) |
| Coordonnées changées | 6 615 | 200 |
| Déplacement moyen, toutes positions reconnues | 4,87 m | 4,74 m |
| 95e centile | 10,82 m | 8,78 m |
| Maximum observé | 11,31 m | 8,78 m |

La distance peut dépasser neuf mètres lorsque les deux coordonnées sont
arrondies : une limite par coordonnée n'est pas celle du déplacement total.

Le jeu ariégeois ne comporte que 73 positions reconnues avec cap/vitesse :
deux caps changent de deux degrés, aucune vitesse ne change. Le petit jeu
arizonien n'en comporte aucune. Cela ne borne pas les effets sur les mobiles.

Sept cas synthétiques vérifient aussi les limites : cap `001` devenu `005`,
vitesse de 300 nœuds devenue 255, hémisphères sud/ouest, horodatage,
altitude en commentaire, conservation d'un statut sans position. Un cap
compressé de huit degrés ressort à douze après arrondi puis réencodage
dans le pas APRS de quatre degrés : ce réencodage peut donc ajouter sa
quantification au pas de cinq degrés du nombre compact.

Zéro erreur PPM ou d'aller-retour de l'entier compact est signalée. Cela
ne signifie pas conservation du paquet original : les changements ci-dessus
sont voulus par `PACKPOS`. Les blocs natifs restent des coûts calculés,
sans encodeur/décodeur RF complet de cet hybride.

## Conclusion

Sur ces jeux, l'arrondi apporte moins d'un octet et environ 1 % de temps
d'antenne par paquet en plus de la position native sans arrondi. La table
de destinations apporte un gain en taille plus important sans imposer
l'arrondi des positions.

Un décalage de quelques mètres peut convenir au suivi GPS ; cet essai ne
justifie pas de l'imposer à toutes les positions relayées. Une copie
reconstruite différemment n'a plus le même champ d'information que l'original :
leur déduplication APRS-IS ne doit pas être supposée. Aucun doublon réel
n'a été mesuré. La décision de format reste ouverte.

Un essai RF nécessite une disposition d'octets et un encodeur/décodeur
correspondants, absents des firmwares actuels des cartes.

## Complément : capture récente sur Firmin

Le 29 octobre, un instantané des deux fichiers de la capture lancée par
Opus sur Firmin a été récupéré sans arrêter ni modifier le service
`n7uv-capture.service`. Il était actif, sans redémarrage systemd depuis
le 28 octobre à 15:37 UTC. La fin reste prévue le 30 octobre à 14:30 UTC.

L'instantané couvre du 28 octobre à 09:41 UTC au 29 octobre à 09:05 UTC,
avec la coupure déjà connue entre 13:00 et 15:37 UTC le 28, et deux
reconnexions de 30 secondes dans les journaux. Il contient 49 725 lignes,
dont 11 505 trames TNC2 reconnues. Le flux contient aussi du trafic APRS-IS :
seules les 9 226 trames accompagnées d'une ligne de métriques `LOCAL` sont
retenues comme réceptions RF locales, provenant de 34 stations.

Parmi ces 9 226 réceptions, 3 924 n'ont aucun saut RXT mesuré dans le flux,
3 325 en ont un, 1 814 deux et 163 trois. Les lignes `NA` ne comptent pas
comme des mesures. Ces nombres décrivent les rapports décodés du flux, pas
une inspection des blocs binaires sur l'air. Ils ne sont pas ajoutés aux
coûts de l'essai présent, qui reste sans RXT.

Après le même partage des stations et le même dédoublonnage consécutif,
le nouveau jeu de test contient **2 659 paquets de 11 stations**. Le modèle
PPM et la liste de destinations sont inchangés : aucune donnée de la
nouvelle capture n'est réinjectée dans l'entraînement.

| Observation sur le jeu récent | Résultat |
| --- | ---: |
| B + table de destinations | 68,45 octets par paquet |
| B + table + position native sans arrondi | 68,35 octets |
| B + table + position arrondie | 67,46 octets |
| Gain marginal de l'arrondi | 0,89 octet |
| Temps d'antenne économisé, SF7 CR 4/6 | 1,62 ms par paquet, soit 1,10 % |
| Paquets dont la durée diminue | 701 sur 2 659 (26,4 %) |
| Positions reconnues | 2 359 |
| Contenu reconstruit modifié | 2 145 (90,93 %) |
| Coordonnées modifiées | 2 141 |
| Déplacement moyen | 2,68 m |
| 95e centile / maximum | 3,92 m / 7,90 m |

Seulement quatre paquets reconnus portent un cap et une vitesse ; leurs
caps changent de un ou deux degrés, aucune vitesse ne change. La capture
élargie reste donc insuffisante pour évaluer les effets sur les mobiles.
Le codec PPM et l'entier compact ne signalent aucun échec. L'ordre de
grandeur du gain marginal reste celui du premier essai, environ 1 % de
temps d'antenne ; cette observation ne valide pas un format RF complet.

Les instantanés et leurs empreintes SHA-256 sont sous
`logs/packed-position-20261029/n7uv-snapshot/` et dans
`arizona_live_extraction.json`. `prepare_live.py` conserve les associations
entre trames et métriques dans `arizona_live_records.jsonl` ; le jeu retenu
par `prep.py` est `arizona_live.tsv`. Le service distant continue sa capture.

## Complément : modèles et liste publiés par Walter

Après son commentaire du 29 octobre à 12:31 UTC, la comparaison a été
reprise au commit `71806d6` de la branche `hybrid`, sans modifier ses
sources. Ce complément utilise sa liste exacte de destinations et les
modèles adaptés à chaque contenu. Il remplace les tailles précédentes
pour cette comparaison avec ses tables ; les observations précédentes
sur les différences de coordonnées restent valables.

Les jeux de test sont inchangés : 8 062 paquets ariégeois, 224 paquets de
l'ancien relevé arizonien et 2 659 du nouveau. Les modèles sont publiés
par Walter et ne sont pas réentraînés sur ces jeux. La liste comporte
77 destinations exactes et 27 motifs de registre, et non 192 entrées
effectivement remplies. Le maximum prévu est de 192 entrées exactes.

### Couverture de sa liste de destinations

| Classe du banc | Ariège, 8 062 paquets | Arizona ancien, 224 | Arizona récent, 2 659 |
| --- | ---: | ---: | ---: |
| Entrée exacte, 1 octet | 6 857 (85,05 %) | 200 (89,29 %) | 2 654 (99,81 %) |
| Motif de registre, 2 octets | 0 | 0 | 0 |
| Destination Mic-E hors liste, coût estimé de 4 octets | 203 (2,52 %) | 1 (0,45 %) | 0 |
| Autre destination hors liste, échappement | 1 002 (12,43 %) | 23 (10,27 %) | 5 (0,19 %) |

Les principales destinations absentes en Ariège sont `APGRWO` (500),
`APFD11` (420), `APDW16` (39) et `APFD12` (30). Les destinations Mic-E
contiennent une partie de la position et ne doivent pas être assimilées
automatiquement à des identifiants de logiciel à ajouter au dictionnaire.
Le détail de toutes les destinations et de leurs effectifs est conservé
dans `results.json` du nouveau banc.

Dans le nouveau jeu arizonien, les cinq échappements sont `APMI06` (4)
et `BEACON` (1). Dans l'ancien : `APN982` (10), `BEACON` (8), `APANC2`
(4), `APDW18` (1), auxquels s'ajoute une destination Mic-E `S3SQTT`.

Sur l'ensemble des **9 226 réceptions RF** de l'instantané récent, avant
partage des stations et dédoublonnage consécutif, la liste couvre
9 215 destinations exactes (99,88 %). Les onze autres sont `APMI06` (4),
`APWL2K` (3), `BEACON` (1), `APU25N` (1) et les deux destinations Mic-E
`SSSRVV` et `SSRWWP`. Ce décompte de couverture ne remplace pas le jeu de
test tenu à l'écart pour la comparaison des tailles.

### Comparaison avec les modèles adaptés

Trames autonomes `N=1`, `NATIVE=1`, `PLAINTEXT=1`, sans RXT ni saut
supplémentaire simulé. PPM du paquet entier : `model_whole.c` ; hybride A :
`model_hybridA_nopath.c` ; B avec position dans le texte :
`model_hybridB_info_native.c` ; B avec position structurée, arrondie ou
non : `model_hybridB_comment.c`. Toutes les variantes à table utilisent
le même fichier `tocall_table.txt` publié par Walter.

Réduction du nombre d'octets par rapport à la trame texte, préfixe radio
de trois octets compris :

| Variante | Ariège | Arizona ancien | Arizona récent |
| --- | ---: | ---: | ---: |
| PPM du paquet entier | 31,90 % | 44,57 % | 39,73 % |
| Hybride A | 33,67 % | 44,41 % | 39,90 % |
| B, position dans le texte | 31,78 % | 44,07 % | 39,03 % |
| B + table, position dans le texte | 36,20 % | 47,37 % | 42,41 % |
| **B + table + position draft 0 sans arrondi** | **38,47 %** | **48,97 %** | **43,78 %** |
| B + table + `PACKPOS=1` | 39,59 % | 49,82 % | 44,53 % |
| B + table + `PACKPOS=2` | 39,58 % | 49,82 % | 44,53 % |

Le B à position non arrondie dépasse le PPM du paquet entier de
6,57 points en Ariège et de 4,05 points sur le nouveau jeu arizonien.
Cela soutient cette combinaison pour la suite des essais sur ces données ;
ce n'est pas une garantie sur d'autres réseaux.

Avec le profil SF12 CR 4/5, la réduction calculée du temps d'antenne est
de 29,74 % pour ce B ariégeois, contre 24,59 % pour le paquet entier.
Avec SF7 CR 4/6 sur le nouveau jeu arizonien, elle est de 38,48 %, contre
34,92 %. Ces durées restent des calculs sur les tailles du banc.

### Option `PACKPOS=2` et limites

Par rapport au B non arrondi avec la même table et le même modèle de
commentaire, `PACKPOS=2` économise encore seulement 0,83 octet par paquet
ariégeois et 0,89 par paquet du nouveau jeu arizonien. Aucun cap ni aucune
vitesse reconstruit ne change dans ces deux jeux avec cette option ; ils
ne contiennent toutefois que 73 et quatre positions avec cap/vitesse.
Cinq cas synthétiques vérifient aussi la conservation de `001/300`,
`000/000`, `360/999`, d'un horodatage avec altitude et d'un statut.
Les coordonnées comprimées restent modifiées : 6 615 contenus de position
sur 7 157 en Ariège et 2 141 sur 2 359 dans le nouveau jeu arizonien.

Les neuf variantes sur les trois jeux ne signalent aucun échec de
décodage PPM ni d'aller-retour de l'entier de position. Toutes les tailles
calculées tiennent dans 255 octets. Cela ne valide pas une trame hybride
complète : les champs structurés sont chiffrés par le banc, sans
sérialisation et réception RF de l'enveloppe commune. Le coût Mic-E
reste une estimation et aucun contrôle d'intégrité supplémentaire n'est
ajouté. Aucun mode primed ni codage WSPR des indicatifs n'est activé.

Le complément reproductible est dans `logs/hybrid-matched-20261029/` :
checkout amont figé, `build.sh`, `measure.c`, `run.py`,
`check_packpos2.py`, résultats par paquet, `results.json` et empreintes
des modèles et des entrées dans `inputs_sha256.json`. Les captures
originales et le service distant ne sont pas modifiés. Depuis la racine :

```sh
sh logs/hybrid-matched-20261029/build.sh
python3 logs/hybrid-matched-20261029/run.py
python3 logs/hybrid-matched-20261029/check_packpos2.py
```

## Complément : table de destinations v2

La liste `tocall_table_v2.txt` est extraite du commit amont `98c367a`.
Les quatre modèles et le code de calcul restent ceux de `71806d6` :
seule la liste change. Les codes et modèles existants ne sont pas
réentraînés. Les trois mêmes jeux de test sont repris en N=1, sans RXT.

| Observation | Ariège | Arizona ancien | Arizona récent |
| --- | ---: | ---: | ---: |
| Entrées exactes | 7 846 / 8 062 (97,32 %) | 208 / 224 (92,86 %) | 2 659 / 2 659 (100 %) |
| Motifs de registre | 0 | 0 | 0 |
| Destinations Mic-E hors liste | 203 | 1 | 0 |
| Autres échappements | 13 | 15 | 0 |
| Gain v2 sur v1, octets par paquet | 0,613 | 0,179 | 0,009 |
| B + table + position sans arrondi, réduction en octets | 39,29 % | 49,14 % | 43,79 % |
| B + table + PACKPOS=2, réduction en octets | 40,40 % | 49,99 % | 44,54 % |

Les 989 nouveaux codes exacts ariégeois expliquent intégralement le
gain : chacun passe de six octets à un, soit 4 945 octets économisés
sur 8 062 paquets. Aucun échec de décodage PPM ou d'entier de position
n'est signalé. Les coûts conservent l'estimation Mic-E du banc pour
permettre la comparaison à modèle et algorithme constants.

Le codeur natif de production `NATIVE_APRS::encode` a été appelé
séparément sur les 203 paquets Mic-E ariégeois et le paquet de l'ancien
jeu arizonien. Chaque élément destination occupe **cinq octets** :
un octet de type/SSID et quatre pour le texte compacté. Il n'existe
pas ici de compactage spécifique de la destination Mic-E sur quatre
octets. Avec une échappatoire de table d'un octet suivie de cet élément,
le coût serait de six, et non les quatre estimés dans le banc.

Les 420 paquets `APFD11` du test proviennent de `F1ZCJ-3` ; les 30
`APFD12` de `F5ZWS-3`. Tous portent `F6DEV` comme première entrée du
chemin. Ces observations ne suffisent pas à identifier le logiciel
émetteur ni à décrire la configuration de la passerelle. Par ailleurs,
`APGRWO` est aussi utilisé par Graywolf dans notre chaîne LoRa ; la
destination n'identifie pas à elle seule une bande d'émission.

Les résultats sont conservés séparément dans
`logs/hybrid-matched-20261029/v2/`. Reproduction depuis la racine :

```sh
TABLE_FILE=logs/hybrid-matched-20261029/v2/tocall_table_v2.txt \
RESULT_DIR=logs/hybrid-matched-20261029/v2 \
python3 logs/hybrid-matched-20261029/run.py
c++ -std=c++17 -O2 -Iinclude \
  logs/hybrid-native-20261028/check_costs.cpp \
  src/native_aprs.cpp src/rxt_protocol.cpp \
  -o logs/hybrid-matched-20261029/v2/check_costs
python3 logs/hybrid-matched-20261029/v2/check_mice.py
```

## Reproduction du premier essai

Les outils et résultats sont dans `logs/packed-position-20261029/` : checkout
amont figé, `measure.c`, `run.py`, `check_synthetic.py`, sorties par paquet,
`results.json` et `synthetic-results.json`. Les captures détaillées restent
locales. Les jeux préparés sont sous `logs/hybrid-review-20261028/`.

Depuis la racine du dépôt, le binaire B se compile ainsi :

```sh
cc -std=c99 -O2 -Ilogs/packed-position-20261029/proposal/reference \
  logs/packed-position-20261029/measure.c \
  logs/packed-position-20261029/proposal/reference/aprs_ppm.c \
  logs/packed-position-20261029/proposal/tools/hybrid/tables/model_hybridB_info.c \
  -lm -o logs/packed-position-20261029/measure_B
```

Les binaires `measure_whole` et `measure_A` utilisent les mêmes sources avec
`model_whole.c` et `model_hybridA_nopath.c`. `run.py` lance les comparaisons,
puis `check_synthetic.py` les cas de contrôle. Aucun mode primed ou WSPR
n'est activé.
