# Évaluer la taille des blocs OTA LoRa

[English](LORA_OTA_BLOCK_BENCH.md)

## Question et critère

Comparer le coût pour livrer et vérifier les mêmes octets, en direct puis
par un relais. Le premier critère est le temps d'antenne total ; la durée
écoulée et la réussite des transferts restent des critères distincts.
Aucune taille optimale n'est annoncée avant les mesures.

Le protocole décrit ici et son banc sont implémentés sur la branche OTA.
Un [essai matériel du 1 novembre](../tests/2026-11-01/OTA_BLOCK_SIZES_fr.md)
valide cinq tailles en direct sur le même échantillon de 8 Ko, avec contrôle
SHA-256 en flash. Il ne remplace pas la matrice comparative ci-dessous.
Les transferts complets et reprises matériels précédents utilisaient les
blocs de 128 octets.

## Variables maintenues et variables comparées

Pour une série, garder le même fichier source, le même préfixe de 65 536
octets, le même SHA-256, les mêmes cartes, le même trajet, les mêmes
paramètres radio, les mêmes antennes et leur orientation. La page flash
reste à 4 096 octets : sa taille et sa fréquence d'écriture ne varient pas
avec les blocs.

Comparer d'abord **32, 64, 96, 128, 160 et 175 octets de données**. On peut
ajouter 192 et 204 octets en direct. L'en-tête authentifié coûte 51 octets ;
l'enveloppe du relais ajoute 29 octets. La charge radio maximale est de
255 octets : les limites sont donc 204 octets en direct et 175 avec relais.
Les derniers blocs d'une page et du fichier peuvent être plus courts.

Le bitmap s'adapte au nombre de blocs de la page : 4, 8, 12 ou 16 octets.
Son surcoût fait partie du résultat. Pour 128 octets, les messages
historiques et le bitmap de quatre octets restent inchangés.

Les retransmissions suivent le même algorithme pour toutes les tailles.
Le relais conserve son cache de page et ses trois tours de réparation
locale. Les délais de réponse sont calculés avec les mêmes formules à
partir du profil et du nombre de blocs ; ils ne sont pas réglés à la main
pour favoriser une taille. En direct, le délai de requête ne dépend pas
de la longueur d'un tour de relayage inexistant.

## Matrice d'essais

1. Liaison directe, signal confortable et environnement décrit.
2. Liaison directe atténuée, avec un obstacle, une distance ou un
   atténuateur documenté ; conserver ce montage pour toutes les tailles.
3. Liaison avec trafic concurrent documenté, si ce trafic peut être
   maintenu comparable entre les passages.
4. Confirmation des tailles retenues avec le relais intermédiaire.

Commencer par cinq répétitions de chaque taille pour chaque condition et
chaque profil. L'ordre est mélangé dans chaque répétition avec une graine
conservée. Les essais d'une autre condition ou d'un autre profil forment
une autre série. Augmenter le nombre de répétitions si la dispersion
empêche de départager les tailles. Un P95 sur cinq passages est seulement
une statistique descriptive, pas une estimation précise des cas rares.

Les conditions sont établies physiquement et décrites par l'opérateur.
Le banc ne transforme pas une perte logicielle volontaire en liaison RF
atténuée. Les anciennes injections de panne ne sont pas utilisées pour
classer les tailles.

Avec un relais, la cible doit attendre pendant que le contrôleur remplit
le cache. Un tour qui dépasse le bail maximal de 60 secondes est refusé
avant l'essai. Certains profils lents, notamment SF12, peuvent donc être
comparés en direct mais nécessitent une évolution de l'ordonnancement pour
être évalués avec le relais. Le banc ne raccourcit pas les pages pour
contourner cette limite et fausser la comparaison.

## Ce que fait un passage

Le contrôleur arme une session authentifiée et vérifie les capacités du
firmware, l’absence de confirmation de démarrage en attente et la configuration
Wi-Fi. Un refus à ce stade ne supprime pas le point de reprise existant ;
la session expire après son délai d’inactivité. Après ces vérifications,
il efface explicitement le précédent point de reprise de laboratoire, puis
ouvre une nouvelle session.
Il négocie le profil temporaire et vérifie que la cible est configurée sans
Wi-Fi ni point d'accès. Le relais garde son accès de diagnostic, si utilisé.

Le chronométrage commence avant WINDOW_START et se termine après la
vérification du préfixe écrit dans la partition inactive. L'échantillon
n'est pas considéré comme une image de firmware valide : il est relu en
flash et son SHA-256 est comparé au manifeste. **Aucune activation ni
confirmation de nouvelle application n'est demandée.**

Après le passage, le contrôleur récupère les compteurs, revient au profil
habituel et ferme la session avec ABORT. Le passage suivant repart de zéro,
sans profiter des pages déjà présentes. Ce nettoyage et la négociation
initiale sont hors de l'intervalle comparé ; leurs traces restent conservées.

La vérification de préfixe est réservée au firmware de laboratoire compilé
avec RFOTA_DIAGNOSTICS. Elle ne place jamais l'image dans l'état READY et
n'autorise pas son activation.

## Comptage et résultats

Le contrôleur compte ses transmissions confirmées par TXDONE. La cible
et le relais comptent leurs émissions réussies, les octets et le temps
sur l'air calculé par RadioLib pour chaque trame réelle. Cela inclut les
réponses perdues par le contrôleur et les réparations locales du relais.
Les requêtes de lecture des compteurs et leurs réponses sont exclues des
compteurs de mesure. Les erreurs du pilote sont signalées et invalident le
passage au lieu d'être assimilées à des émissions réussies.

Ce **temps d'antenne est calculé**, pas mesuré par un analyseur RF.
Il utilise le nombre d'émissions effectivement confirmées par les radios,
les tailles complètes sur l'air et les profils actifs ; il ne correspond
pas à un simple volume utile divisé par le débit nominal.

La durée est mesurée par une horloge monotone. Les compteurs de démarrage
permettent de refuser un passage dont la cible ou le relais a redémarré.
Le changement de nonce n'est pas présenté comme une réussite de transfert.

Conserver pour chaque passage : taille des blocs, profil et condition,
répétition et ordre, volume et SHA-256, réussite ou erreur, durée totale,
temps d'antenne cumulé sur tous les émetteurs, nombre de transmissions du
contrôleur, de la cible et du relais, retransmissions du contrôleur et
réparations locales. Les résultats enregistrent aussi les empreintes ELF annoncées par la cible
et le relais pour identifier les firmwares exécutés. Conserver les binaires
du pont et des équipements avec leurs SHA-256 pour la série. Les traces
JSONL contiennent les requêtes et réponses sans exporter les clés.

Une panne arrête la série ; le passage échoué reste dans trials.json et
trials.csv. Il ne disparaît pas dans une moyenne. Le résumé distingue le
nombre de passages tentés et réussis et donne moyenne, médiane, P95 et
maximum des passages réussis. Une série incomplète ne désigne pas de gagnant.

## Préparer sans toucher aux cartes

Depuis le worktree OTA :

```bash
python3 tools/lora_ota/block_bench.py plan \
  --radio-profiles tools/lora_ota/radio-profiles.example.json \
  --blocks 32,64,96,128,160,175 --repeats 5 --seed 20261101 \
  --condition 'banc direct, antennes et distances à documenter' \
  --output logs/block-bench-20261101/plan-direct.json
```

Ce mode ne lit aucun secret, n'ouvre aucun port et ne contacte aucun équipement.
Les temps d'antenne indiqués sont des minima théoriques par page, sans pertes,
sans négociation et sans délai de traitement ; ce ne sont pas des résultats RF.

## Installer le banc et exécuter

Les cibles et relais doivent porter les nouveaux firmwares compatibles.
Sauvegarder leurs configurations, leurs clés et leurs flashs avant toute
installation. Choisir le montage de test et ses profils avant de flasher.
L'inventaire distingue les identités radio et les accès de gestion ; MLV-10
est au bout de son mât, et non une carte dont on suppose l'USB disponible.

```bash
pio run -e ttgo-lora32-v21-ota-a -e ttgo-lora32-v21-ota-relay
pio run -d tools/lora_ota -e bridge_fast
```

Après provisionnement de laboratoire du pont et de la cible :

```bash
python3 tools/lora_ota/block_bench.py run \
  --bridge /dev/serial/by-id/PORT_DU_PONT \
  --key-file CHEMIN_PRIVE/secret.base64 \
  --counter-file CHEMIN_PRIVE/compteur \
  --image CHEMIN_IMAGE/firmware.bin --sample-bytes 65536 \
  --radio-profiles tools/lora_ota/radio-profiles.example.json \
  --blocks 32,64,96,128,160,175 --repeats 5 --seed 20261101 \
  --condition 'description précise du montage direct' \
  --output logs/block-bench-20261101/direct-signal-confortable
```

Pour un relais, ajouter `--via F4MLV-10`, `--route-key-file CHEMIN_PRIVE/route.base64`
et `--relay-status-url URL_DE_GESTION/lab/ota/route`. Le dernier paramètre
est une adresse de gestion réellement accessible, éventuellement par tunnel
SSH ; il ne représente pas un nouveau transport des blocs. Le relais doit
annoncer window_geometry=2 et fournir ses compteurs, sans quoi le banc refuse
la série. Son absence de support ne devient pas une série de pertes RF.

Les résultats, captures, clés, sauvegardes et fichiers de compteurs restent
dans des dossiers locaux ignorés par Git. Le répertoire d'une série doit
être nouveau : le banc refuse de recouvrir une série existante.

## Extension du protocole

- WINDOW_START historique : 40 octets de manifeste, blocs de 128 octets.
- WINDOW_START étendu : même manifeste suivi de la taille sur deux octets
  big-endian, entre 32 et 204 ; 175 maximum en passant par le relais.
- WINDOW_QUERY et le bitmap de sa réponse utilisent la largeur calculée
  pour une page complète. Un bitmap est un entier big-endian dont le bit
  zéro correspond au premier bloc. Les bits hors de la page finale restent nuls.
- L'alignement des blocs repart à chaque page de 4 Ko, y compris lorsque
  la taille ne divise pas 4 096. Un bloc ne traverse jamais une page.
- La reprise durable reste un offset de page indépendant de la taille des
  blocs. Une nouvelle session peut adopter une autre taille au même point
  de reprise ; une session active refuse un changement de géométrie.
- Opcode 25 : lecture authentifiée des compteurs TX et d'un identifiant
  de démarrage. Opcode 26 : vérification SHA-256 d'un échantillon écrit,
  disponible seulement sur les cibles de diagnostic ; aucune activation.

Les anciens échanges de 128 octets restent compatibles. Les échanges
étendus exigent des équipements mis à jour sur tout le trajet. Le contrôleur
vérifie aussi la largeur du bitmap annoncée par la cible.

## Décision après mesures

Retenir les tailles qui minimisent le temps d'antenne avec une fiabilité
acceptable, puis comparer leurs durées et leur dispersion. Confirmer ces
choix par des transferts d'images complètes avec le chemin de déploiement
prévu. Le client send_integrated.py accepte `--windowed --block-size N`
pour cette étape ; sa vérification complète et son activation restent
celles du protocole OTA normal.

Un résultat sur deux cartes proches ne choisit pas à lui seul la taille
pour un relais éloigné. L'adaptation automatique de la taille en cours de
transfert n'est pas implémentée par ce banc.

## Référence

[Malumbres et al., Sensors 2024, 24(7), 2104](https://pmc.ncbi.nlm.nih.gov/articles/PMC11014374/)
étudie notamment l'effet de la taille des fragments et des réparations sur
la durée de distribution LoRa. Ses résultats sur plusieurs destinataires
et à débit fixe ne déterminent pas l'optimum de notre chemin relayé.
