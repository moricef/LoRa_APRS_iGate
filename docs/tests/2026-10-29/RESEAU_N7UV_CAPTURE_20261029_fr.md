# Réseau N7UV : premières observations de la capture des 28–29 octobre

Analyse du 29 octobre 2026, indépendante du banc de compression. Elle porte
sur le même instantané des fichiers enregistrés sur Firmin. Aucun réglage,
firmware ou service distant n'a été modifié.

## Périmètre et comptage

Du 28 octobre à 09:41 UTC au 29 octobre à 09:05 UTC, avec la coupure connue
entre 13:00 et 15:37 UTC le 28 et deux reconnexions de 30 secondes.
Le point d'observation est le flux public `n7uv1.duckdns.org:33001` ;
son identité de récepteur local n'est pas déduite d'un indicatif voisin.
Les mesures des relais RXT sont transportées jusqu'à ce point, pas prises
localement par lui.

Les 49 725 lignes comprennent 11 505 trames TNC2. Seules les 9 226
accompagnées de métriques `LOCAL` sont retenues comme réceptions RF.
Elles proviennent de 34 identités sources. Les lignes APRS-IS sans ces
métriques sont exclues. Aucun partage entraînement/test n'est appliqué ici.

Les tuples RXT sont souvent recopiés dans plusieurs branches de relayage.
Pour ne pas traiter ces copies comme des mesures indépendantes, les rapports
de même source, destination, information exacte, liaison et quadruplet
RSSI/SNR/FO/TTH sont regroupés sur 30 secondes à partir du premier reçu.
Cela réduit 7 442 rapports chiffrés à 5 554 observations. C'est une
heuristique : deux réceptions réellement distinctes mais identiques dans
cette fenêtre pourraient être regroupées. Les données restent biaisées
vers les paquets parvenus au collecteur.

## 1. L'asymétrie SOMTNP–WHTANK reste observable

Les flèches vont de l'émetteur au récepteur du saut. Les moyennes portent
sur les observations regroupées selon la règle précédente.

| Liaison | Observations | RSSI moyen | SNR moyen | SNR négatif |
| --- | ---: | ---: | ---: | ---: |
| SOMTNP → WHTANK | 190 | −78,94 dBm | +3,48 dB | 75 sur 190 |
| WHTANK → SOMTNP | 144 | −77,90 dBm | +10,39 dB | 0 sur 144 |

L'écart de RSSI moyen est voisin d'un dB, celui du SNR d'environ sept dB.
Chez WHTANK, le SNR de SOMTNP va de −9,5 à +12,5 dB ; dans l'autre sens,
il reste entre +2 et +13 dB. Cela prolonge le constat signalé par Jon.
Ces mesures ne suffisent pas à attribuer la cause à un brouillage plutôt
qu'à un autre effet de réception ou de mesure.

Un contrôle avec un même émetteur donne un résultat concordant : pour
CAVECR reçu par SOMTNP et WHTANK, 120 groupes de même contenu dans la
fenêtre de 30 secondes disposent de mesures des deux récepteurs. Le SNR
est en moyenne supérieur de 5,98 dB chez SOMTNP, alors que son RSSI est
inférieur de 1,23 dB. Les rapports sont associés au même contenu, sans
preuve qu'ils correspondent exactement à la même émission de CAVECR.
Le contrôle appuie le constat, sans établir la cause ni généraliser à
tous les émetteurs : BLACK, par exemple, est mieux reçu chez WHTANK.

## 2. De nombreuses copies du même contenu arrivent par plusieurs chemins

La clé est exactement `(source, destination, information)`, sans chemin,
dans une fenêtre ancrée de 30 secondes. Aucun suffixe n'est retiré pour
ce comptage.

- 9 226 réceptions correspondent à 2 911 groupes de contenu ;
- 2 145 groupes apparaissent au moins deux fois ;
- la moyenne est de 3,17 réceptions par groupe, la médiane de trois ;
- le maximum est de huit copies en six à huit secondes.

Exemple : une trame de N7UV-6 à 23:44:45 UTC le 28 apparaît huit fois
en huit secondes, notamment en direct selon son chemin, puis via BURNTX,
LMOSCA, SOMTNP, WHTANK et plusieurs combinaisons de relais.

Les 6 315 réceptions au-delà de la première d'un groupe ne sont pas
6 315 transmissions démontrées inutiles : elles peuvent participer à la
couverture d'autres zones. Le flux ne permet pas non plus d'affirmer
qu'elles ont toutes été envoyées sur APRS-IS. Il mesure la multiplicité
observée à ce récepteur, pas une perte de capacité globale.

## 3. Des chemins répètent un indicatif de relais

105 réceptions portent au moins deux fois le même indicatif parmi les
entrées utilisées du chemin. Exemples :

| Réception UTC le 28 octobre | Source | Chemin |
| --- | --- | --- |
| 09:51:57 | PINAL | `CAVECR,W9EN-44,CAVECR*` |
| 09:56:48 | ONIONM | `SUNSET,SUNSET*,WIDE2-1` |

Ces répétitions méritent une vérification des règles de relayage et des
identités configurées. Elles ne prouvent pas seules une boucle physique :
plusieurs équipements partageant un identifiant ou une réécriture du chemin
peuvent aussi produire ce résultat. Aucun firmware précis n'est mis en
cause par cette capture seule.

## 4. Des suffixes visibles modifient encore le champ d'information

763 trames finissent par quatre caractères imprimables entre accolades,
une forme compatible avec un ancien trailer RXT v1. Cette forme seule
ne permet pas d'affirmer que tout suffixe est du RXT.

Pour 741 d'entre elles, une autre réception de même source, destination
et contenu de base, sans le suffixe, existe à moins de 30 secondes.
Exemple à 09:41:53 UTC : PINAL arrive via `WHTANK*,WIDE2-2`, puis une
copie à 09:42:03 via `W0KJH-10,BLACK*,WIDE2-1` porte le suffixe visible.
Cela situe une différence de contenu sur cette branche, sans identifier
lequel de ses équipements l'a ajoutée.

Les deux champs d'information étant différents, la déduplication APRS-IS
ne doit pas être supposée si ces copies y sont injectées. Le flux ne
démontre pas cette injection. C'est un cas concret de compatibilité à
examiner avec Jon, distinct du simple changement de chemin.

## 5. Les TTH rapportés diffèrent fortement selon le relais

Cinq identifiants de récepteurs fournissent des mesures chiffrées.

| Récepteur | Observations RXT | TTH médian | 95e centile |
| --- | ---: | ---: | ---: |
| SOMTNP | 1 955 | 3 542 ms | 4 133 ms |
| WHTANK | 1 276 | 3 542 ms | 4 133 ms |
| PINAL | 149 | 3 278 ms | 3 542 ms |
| BURNTX | 1 347 | 342 ms | 695 ms |
| LMOSCA | 827 | 401 ms | 643 ms |

Les TTH sont décodés et quantifiés, pas chronométrés directement par le
collecteur. Ils comprennent réception achevée → émission achevée, donc la
file, les reports CAD et la durée de transmission. Les profils radio et
versions réellement installés doivent être confirmés avant d'interpréter
l'écart comme une différence d'encombrement ou comme un défaut de calcul.

## Trafic et limites

Les positions, horodatées ou non, représentent 8 756 réceptions et 95,18 %
des octets TNC2 observés. Ce n'est pas une part mesurée de temps d'antenne :
le flux a déjà transformé les trames radio et séparé leurs métadonnées.
Le chemin textuel mesure 17,14 octets en moyenne, pour 104,60 octets TNC2.
Les 188 trames tierces contiennent des objets. Les 251 messages de
définition de télémétrie ne constituent pas un essai de messagerie humaine.

Les positions publiées par les stations permettent de calculer des distances
cartographiques, présentes dans le CSV, mais ne prouvent pas les positions
physiques des équipements. Une carte de test peut annoncer son futur site.
Aucun record DX physique n'est donc revendiqué ici.

Le flux contient uniquement les paquets exportés par le collecteur et les
mesures RXT arrivées jusqu'à lui. Sans nombre de transmissions, compteurs
de CRC rejetés et observations aux autres sites, il ne permet pas de
calculer un taux de pertes, de mesurer les collisions ou d'établir une
fiabilité de livraison de bout en bout. `NA` n'identifie pas sa propre
cause : relais non équipé, mesure perdue, format incompatible ou autre.

## Artefacts et reproduction

Le script est `logs/n7uv-20261028/analysis-20261029/analyze.py`.
Il lit l'instantané figé sous `logs/packed-position-20261029/`, dont les
fichiers et empreintes sont décrits dans `arizona_live_extraction.json`.

Il produit dans son dossier :

- `results.json` et `summary.txt` : résultats et limites de comptage ;
- `links.csv` : qualité et TTH par liaison dirigée ;
- `link_samples.csv` : observations RXT regroupées ;
- `hourly_links.csv` : évolution par heure UTC de réception au collecteur ;
- `multiple_copies.csv` : groupes à plusieurs copies et leurs chemins.

Les captures et ces exports restent locaux. Pour reproduire :

```sh
python3 logs/n7uv-20261028/analysis-20261029/analyze.py
```
