# Proposition RXT v2 — tuples auto-identifiés

## Pourquoi

Un tuple RXT v1 n'indique pas quel digi l'a produit. Pour l'attribuer, le
décodeur rapproche les tuples d'une liste blanche d'indicatifs RXT présents
dans le chemin. Quand cette liste est incomplète ou diffère d'un iGate à
l'autre, les sauts s'affichent en N/A ou sont attribués au mauvais digi.

Il y a aussi l'ambiguïté du trailer dont on a parlé : un commentaire de balise
qui se termine par quelque chose comme `{abcd}` ressemble exactement à des
données RXT. Le digi y ajoute son tuple, et l'iGate le retire avant APRS-IS :
le texte de l'utilisateur est perdu. Le risque est faible, mais la perte est
silencieuse et peut toucher une station de n'importe quelle marque.

## Format

Ajouter un caractère devant chaque tuple, qui identifie le digi qui l'a
produit :

```
v1 : RSSI SNR FO TTH        4 caractères
v2 : ID RSSI SNR FO TTH     5 caractères
```

RSSI, SNR, FO (écart de fréquence) et TTH (temps de relais) sont encodés
exactement comme en v1. L'indicatif est déjà dans le chemin ; `ID` ne sert
qu'à le retrouver.

J'ai d'abord pensé à utiliser la position du digi dans le chemin, mais ça ne
tient plus dès qu'un digi réécrit le chemin (`WIDE1*`/`WIDE2*` consommés
retirés, alias remplacé par `CALL*,WIDE2-1`), ou qu'un digi décrémente un
alias sans inscrire son indicatif. `ID` est donc plutôt une empreinte de
l'indicatif.

## Empreinte

Prendre l'indicatif tel qu'il figure dans le chemin : en majuscules, SSID
compris, l'indicatif tactique si le digi en utilise un, sans l'`*`. Calculer
FNV-1a 32 bits sur ses octets (départ `0x811C9DC5`, pour chaque octet un XOR
puis une multiplication par `0x01000193`, modulo 2^32), prendre le résultat
modulo 89 et ajouter 33. On obtient un caractère entre `!` et `y`, dans la
même plage que les autres champs.

Vecteurs de test :

| Indicatif | FNV-1a | mod 89 | ID |
| --- | --- | --- | --- |
| F4MLV-MC | `0xDBB682C7` | 40 | `I` |
| F4MLV-18 | `0xDDC21C0E` | 34 | `C` |
| F6DEV-10 | `0x6905079A` | 34 | `C` |
| F4MLV-2 | `0xD182B2AF` | 9 | `*` |
| F1ZDB-10 | `0x634631E2` | 9 | `*` |
| F4MLV-10 | `0xD5C20F76` | 71 | `h` |
| F5ZQC-10 | `0x237100C0` | 32 | `A` |

Personne n'attribue l'ID. Chaque digi calcule le sien à partir de son
indicatif : pas de registre, pas de coordination entre digis, rien qui puisse
s'épuiser. L'ID n'a pas à être unique dans une zone : l'indicatif complet est
déjà dans le chemin, et l'ID ne départage que les deux ou trois digis d'un même
chemin.

Sur 133 indicatifs relevés dans mes journaux, 1,28 % des paires ont le même
ID, proche du 1/89 attendu. Quand deux digis d'un même chemin ont le même ID
et ont tous deux ajouté un tuple, les tuples sont attribués dans l'ordre du
chemin, puisqu'ils sont ajoutés dans l'ordre des relais. Si l'un d'eux n'a pas
ajouté de tuple, celui qui reste peut être attribué au mauvais digi : la
mesure est alors mal attribuée, pas perdue. Un tuple dont l'ID ne correspond à
rien dans le chemin (digi qui n'a pas inscrit son indicatif) est affiché comme
relais non identifié plutôt que deviné ; l'empreinte ne peut pas retrouver un
indicatif absent du chemin.

Ces règles doivent être écrites précisément, puisque tout autre logiciel qui
lira v2 devra les reproduire à l'identique.

## Contrôle du trailer

La même empreinte permet au firmware de distinguer des données RXT d'un
commentaire. Un trailer n'est traité comme RXT que si chaque tuple désigne un
digi réellement utilisé dans le chemin. Le contrôle sert en relais comme au
passage vers APRS-IS :

- en relais : si le `{...}` existant échoue au contrôle, le digi n'y touche
  pas et ouvre un nouveau `{...}` après lui ;
- au passage vers APRS-IS : le `{...}` final n'est retiré que s'il passe le
  contrôle.

Un commentaire terminé par `{abcde}` devrait correspondre par hasard à un digi
utilisé : le risque devient très faible des deux côtés. Il n'est pas nul, mais
en v1 un commentaire qui se termine comme `{abcd}` est toujours pris pour du
RXT.

## Pourquoi ni la position, ni une liste, ni une balise de capacité

Savoir quels digis sont compatibles RXT ne suffit pas pour attribuer les
tuples, même si tous les digis du réseau l'étaient :

- un digi peut relayer un paquet sans ajouter de tuple, quand le tuple ferait
  dépasser la taille maximale d'une trame LoRa (le firmware relaie alors le
  paquet tel quel) ;
- les chemins sont réécrits (`WIDE1*`/`WIDE2*` consommés retirés, alias
  décrémentés sans inscription d'indicatif) : le n-ième tuple n'est pas
  forcément le n-ième digi.

Dans les deux cas, les tuples qui suivent le trou glissent sur les mauvais
digis, et rien dans le paquet ne le montre. Il en va de même au passage vers
APRS-IS : sans ID, l'iGate doit deviner d'après une liste si le `{...}` final
est du RXT ou le texte de l'utilisateur, et deux iGates aux listes différentes
envoient des paquets différents.

Une balise de capacité dit ce qu'un digi sait faire, pas ce qu'il a fait sur
ce paquet. Une balise locale sans chemin n'est entendue que par les stations
qui entendent ce digi en direct, et rien ne garantit que l'iGate qui décode
les tuples en fasse partie. Elle reste utile pour la découverte et peut
coexister avec v2 ; le décodage n'en dépend simplement pas.

L'ID n'est pas une mesure du récepteur. Il est là pour que chaque paquet
contienne ce qu'il faut pour le lire, sans état conservé dans le réseau.

## v1 et v2 ensemble

Avec 3 tuples au plus, les longueurs ne se chevauchent jamais : 4, 8 ou 12
caractères pour v1, 5, 10 ou 15 pour v2. Trois suffisent largement avec le
New-N paradigm (`WIDE1-1,WIDE2-1` au plus, `WIDE2-2` pour une station fixe, et
le fork limite les alias régionaux à 2 sauts par défaut).

Seules nos cartes utilisent RXT aujourd'hui : passer à v2, c'est les reflasher
toutes et abandonner la liste blanche. Un iGate v2 peut encore reconnaître un
trailer v1 résiduel à sa longueur et l'ignorer.

Graywolf ne voit que les sauts déjà décodés par l'iGate : le changement de
format ne le concerne pas, sauf éventuellement pour afficher un relais non
identifié.

## Temps d'antenne

Un octet de plus par tuple. Rapporté au seul champ RXT, c'est un octet sur
cinq ; ce qui compte sur le canal, c'est la durée de la trame entière.

Sur des trames réelles : le journal SD de F4MLV-10 contient 12 490 trames
qu'il a émises avec un trailer RXT (12 320 en v1, 170 en v2 ; 11 977 à un
tuple, 511 à deux, 2 à trois). En gardant la taille et le nombre de tuples de
chaque trame, et en calculant sa durée avec 4 puis 5 caractères par tuple :

| Profil | Trame v1 moyenne | Surcoût v2 | Trames allongées | Bloc |
| --- | --- | --- | --- | --- |
| EU : SF12, 125 kHz, CR 4/5 | 3 383 ms | +0,72 % | 15 % | 164 ms |
| SF7, 125 kHz, CR 4/6 | 168 ms | +1,35 % | 37 % | 6,1 ms |

C'est un calcul sur des trames v1 émises, pas une mesure du trafic v2. Les
trames viennent de notre réseau ; des trames aux commentaires plus longs
donnent un pourcentage plus faible.

Même calcul sur ton réseau, à partir du flux TNC public
`n7uv1.duckdns.org:33001` le 29 septembre 2026, de 15:14 à 16:54 UTC : 957
trames de 39 stations, dont 384 avec des tuples (322 à un tuple, 61 à deux, 1
à trois). Le flux montre les trames sans leur trailer et donne les sauts
décodés à part : le nombre de tuples vient des lignes de saut qui ne sont pas
`NA`, et le trailer v1 est reconstitué à 4 caractères par tuple plus les
accolades.

| Profil | Trame v1 moyenne | Surcoût v2 | Trames allongées | Bloc |
| --- | --- | --- | --- | --- |
| SF7, 125 kHz, CR 4/6 | 238 ms | +0,87 % | 34 % | 6,1 ms |

Sur des tailles de paquets synthétiques de 40 à 220 octets, profil EU :

| Tuples | Moyenne | Max | Paquets concernés |
| --- | --- | --- | --- |
| 1 | +33 ms (0,7 %) | +164 ms | 19 % |
| 2 | +65 ms (1,4 %) | +164 ms | 39 % |
| 3 | +98 ms (2,0 %) | +164 ms | 59 % |

Les trames LoRa s'allongent par blocs (164 ms en SF12) : la plupart des
paquets ne s'allongent pas du tout, les autres prennent un bloc. Une balise de
90 octets dure environ 4 s.
