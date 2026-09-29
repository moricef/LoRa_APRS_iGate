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

89 valeurs ne suffisent évidemment pas à distinguer tous les indicatifs du
monde, mais ce n'est pas nécessaire : l'empreinte ne départage que les deux ou
trois digis d'un même chemin. Sur 133 indicatifs relevés dans mes journaux,
1,28 % des paires entrent en collision, proche du 1/89 attendu. Quand deux
digis d'un même chemin ont le même ID, les tuples sont attribués dans l'ordre
du chemin, puisqu'ils sont ajoutés dans l'ordre des relais. Un tuple dont l'ID
ne correspond à rien dans le chemin (digi qui n'a pas inscrit son indicatif)
est affiché comme relais non identifié plutôt que deviné. L'attribution ne
peut échouer que si deux digis de même ID sont dans le chemin et que l'un n'a
pas ajouté de tuple : la mesure est alors mal attribuée, pas perdue.

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
utilisé : le risque devient très faible des deux côtés.

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

Un octet de plus par tuple. Au profil EU (433,775 MHz, SF12, 125 kHz, CR 4/5),
sur des paquets de 40 à 220 octets. Un autre profil donne d'autres valeurs
absolues, mais des pourcentages voisins :

| Tuples | Moyenne | Max | Paquets concernés |
| --- | --- | --- | --- |
| 1 | +33 ms (0,7 %) | +164 ms | 19 % |
| 2 | +65 ms (1,4 %) | +164 ms | 39 % |
| 3 | +98 ms (2,0 %) | +164 ms | 59 % |

Les trames LoRa s'allongent par blocs (164 ms en SF12) : la plupart des
paquets ne s'allongent pas du tout, les autres prennent un bloc. Une balise de
90 octets dure environ 4 s.
