# Trame LoRa APRS native — brouillon 0

Voici la disposition des octets implémentée dans `src/native_aprs.cpp`. C'est
un brouillon de travail à discuter, pas un format arrêté. Les raisons d'une
trame native sont dans `LORA_APRS_NATIVE_FORMAT_fr.md`.

## Principe

Une trame native transporte un paquet TNC2 ordinaire sous forme binaire. Son
décodage redonne le paquet d'origine octet pour octet : un iGate envoie donc
exactement ce qu'aurait produit une trame texte, et APRS-IS peut toujours
reconnaître les doublons. Ce que le brouillon ne sait pas représenter
exactement voyage en texte.

L'encodeur le vérifie lui-même : il décode chaque trame qu'il construit, et si
le résultat diffère de l'entrée, il se replie sur le transport du champ
d'information entier en texte.

Sur les corpus locaux (26 303 réceptions de F4MLV-10 et 1 212 trames RF du
flux N7UV), tous les paquets font l'aller-retour exact. Le temps d'antenne
baisse de 25,3 % (SF12, CR 4/5) et de 21,9 % (SF7, CR 4/6).

## Trame

```
'<' 0xFF 0x02 | en-tête | source | destination | chemin... | [RXT] | [position] | texte
```

Le préfixe ne diffère de celui de la trame texte (`<` `0xFF` `0x01`) que par
son dernier octet. Le firmware upstream actuel vérifie le préfixe et ignore
les trames natives. Les builds de ce fork sans support natif ne le vérifient
pas : lues comme du texte et coupées au premier octet nul, 88 % des trames du
corpus n'ont pas de `>` et sont abandonnées sans trace ; les 12 % restantes
donnent un expéditeur qui n'est pas un indicatif, et ne sont donc ni relayées
ni envoyées sur APRS-IS, mais peuvent laisser une ligne illisible dans le
journal SD, la liste de la WebUI et le flux JSON.

Les entiers sur plusieurs octets sont en gros-boutiste.

### Octet d'en-tête

| Bits | Signification |
| --- | --- |
| 7–4 | nombre d'éléments de chemin, de 0 à 15 |
| 3 | un bloc RXT suit le chemin |
| 2 | le texte est codé en Huffman |
| 1–0 | type d'information : 0 brut, 1 position |

## Éléments d'adresse

La source, la destination et chaque élément de chemin utilisent le même
codage, choisi par le premier octet.

| Premier octet | Élément | Octets suivants |
| --- | --- | --- |
| `0 s nnn NNN` | `WIDEn-N`, avec `*` si s = 1 ; N = 0 signifie `WIDEn` sans `-N` | aucun |
| `1 0 s xxxxx` | indicatif de 1 à 6 caractères `A-Z0-9`, `*` si s = 1 | 4 |
| `1 1 lllll` | tout autre élément, en texte | l (1 à 31) |

Pour un indicatif, les 4 octets sont les 6 caractères, complétés par des
espaces, en base 40 avec l'alphabet `␠A–Z0–9` (espace = 0, A = 1, 0 = 27).
`xxxxx` est le SSID : 0 signifie aucun, 1 à 30 est le SSID, 31 signifie que le
SSID (31 à 255) est dans un octet supplémentaire après les 4 octets. Un SSID
avec un zéro initial (`-0`, `-05`) passe en texte.

Les éléments qui ne sont pas sous ces formes passent en texte : SSID
alphabétiques (`F4MLV-MC`), minuscules, ou chemins mal formés vus sur l'air
(`WIDE2*2`, `F6DEV-10**`).

## Bloc RXT

Présent quand le bit 3 de l'en-tête est à 1.

```
masque | tuple du bit le plus bas | tuple du suivant | ...
```

Le bit k du masque signifie que l'élément de chemin k (0 = premier élément
après la destination) porte une mesure. Chaque tuple fait 4 octets : RSSI,
SNR, FO et TTH, chacun étant le caractère RXT v1 moins 33. Seuls les 8
premiers éléments du chemin peuvent porter un tuple.

La mesure est rattachée à son relais par sa position dans la structure. Il
n'y a ni caractère d'identification ni accolades : elle ne peut pas être prise
pour un commentaire. Le bloc coûte 1 + 4n octets pour n tuples, contre 2 + 4n
pour le trailer v1 et 2 + 5n pour la v2.

## Position

Utilisée pour les champs d'information commençant par `!`, `=`, `/` ou `@`
dont le brouillon sait reproduire exactement la disposition. L'octet de
drapeaux vient en premier.

| Bit | Signification |
| --- | --- |
| 1–0 | type de donnée : 0 `!`, 1 `=`, 2 `/`, 3 `@` |
| 2 | horodatage présent (3 octets) |
| 3 | position compressée (sinon non compressée) |
| 4 | cap/vitesse présents (3 octets, non compressée uniquement) |
| 5 | altitude présente (4 octets) |
| 6 | compressée : les octets cap/vitesse sont deux espaces |

Puis, dans cet ordre :

- **Horodatage** : 6 chiffres × 4 + format, format 0 `z`, 1 `h`, 2 `/`.
- **Position compressée** : latitude et longitude sous forme de leurs valeurs
  base 91, 27 bits chacune, regroupées sur 7 octets (latitude d'abord) ;
  octet de table de symbole ; octet de code de symbole ; puis soit l'octet T
  (bit 6 à 1), soit les trois octets `c s T`.
- **Position non compressée** : 6 octets contenant, depuis les bits de poids
  fort, la latitude en centièmes de minute (20 bits) et un bit sud, puis la
  longitude en centièmes de minute (21 bits) et un bit ouest ; octet de
  table de symbole ; octet de code de symbole. Les positions ambiguës
  (espaces au lieu de chiffres) passent en texte.
- **Cap/vitesse** : cap × 1024 + vitesse, 3 octets, pour une extension
  `ddd/ddd` placée juste après la position.
- **Altitude** : position de `/A=` dans le commentaire (1 octet), puis
  l'altitude en pieds sous forme d'entier signé sur 24 bits. Le décodeur remet
  `/A=` à cette position, avec 6 chiffres, ou `-` et 5 chiffres.

Le commentaire restant, sans `/A=`, constitue le texte.

## Texte

La fin de la trame. Si le bit 2 de l'en-tête est à 0, il est recopié tel
quel. S'il est à 1, un octet donne la longueur du texte, suivi du texte codé
avec un code de Huffman canonique, complété par des bits à zéro jusqu'à
l'octet entier.

Les longueurs de code sont fixes et font partie du format :
`include/native_aprs_huffman_table.h`, 256 entrées, 15 bits au plus. Les codes
sont attribués dans l'ordre (longueur, valeur de l'octet). L'encodeur n'utilise
Huffman que si le résultat, octet de longueur compris, est plus court que le
texte.

La table a été construite à partir du texte des deux corpus. La changer
change le format.

## Vecteurs de test

`test/native_aprs_host/vectors.txt` contient 54 cas tirés des corpus et
quelques-uns construits à la main. Chaque ligne donne les tuples RXT, le
paquet TNC2 en hexadécimal et la trame native attendue en hexadécimal. Toute
autre implémentation doit les reproduire exactement.

## Support dans le firmware

L'iGate reçoit les trames natives : il les décode en TNC2 et transforme le
bloc RXT en trailer v2, si bien qu'APRS-IS, le digipeater, le TNC, le JSON et
le journal SD fonctionnent sans changement. `lora.txFormat` choisit ce qu'il
émet : 0 texte (par défaut), 1 natif, 2 texte puis natif. En émission native,
le trailer v2 du paquet passe dans le bloc RXT, et un paquet impossible à
encoder part en texte.

Essai RF du 29 septembre 2026, F4MLV-2 et F4MLV-15 (`ttgo-lora32-v21_SD`) :

- un statut et une position compressée émis en natif par F4MLV-2 ont été
  décodés par F4MLV-15 et relayés avec son tuple RXT, octet pour octet ;
- les deux émettant en natif, F4MLV-2 a émis 34 octets au lieu de 45 et
  décodé le relais natif de F4MLV-15, bloc RXT compris ; le TTH mesuré par
  F4MLV-15 était de 2 095 à 2 499 ms, contre environ 2 725 ms en relais texte ;
- F4MLV-2 a envoyé la trame relayée sur APRS-IS sans trailer, un commentaire
  `{world}` intact, et aucun autre iGate n'en a envoyé de copie.

## Points ouverts

- Mic-E, messages, télémétrie et objets voyagent en texte. Ils représentent
  environ 12 % du temps d'antenne chez F4MLV-10 et 17 % sur le flux N7UV.
- L'état du relais est le chemin TNC2, avec ses marques `*`. Un état de relais
  natif (demandé, utilisé, restant) n'est pas encore défini.
- Une trame native est relayée en passant par le texte : la logique de relais
  travaille sur le chemin TNC2.
- Le tracker traite les trames natives sur la branche `feature/native-frames`
  de `moricef/LoRa_APRS_Tracker` (commit `4ced9cc7`), avec une copie de ce
  codec. Essai avec le T-Deck F4MLV-7 et F4MLV-2 le 29 septembre 2026 : une
  balise Mic-E (29 octets au lieu de 44), un message avec reply-ack (35 au lieu
  de 52) et son accusé (31 au lieu de 48) ont été décodés dans les deux
  sens.
