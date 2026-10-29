# Banc OTA LoRa direct — prototype du 29 octobre 2026

Ce programme expérimental transfère une image d'application ESP32 par
radio vers une partition OTA inactive. Il est autonome et ne remplace
pas une implémentation OTA dans le firmware iGate. Il n'est pas destiné
aux sites inaccessibles à ce stade.

## Périmètre

- TTGO LoRa32 V2.1, ESP32 classique et SX1278.
- Un émetteur relié en USB au PC et un récepteur également accessible
  en USB pour l'armement et l'observation.
- Profil fixe : 434,500 MHz, BW500, SF7, CR 4/5, puissance 2 dBm,
  préambule 8, CRC LoRa activé. Le SX1278 ne permet pas SF5.
- Deux partitions d'application de `0x1e0000` octets, conformément à
  `min_spiffs.csv`. Aucun changement des partitions entre les deux images.
- Aucun relais, changement automatique de profil, patch différentiel,
  ordonnanceur de coexistence APRS ou reprise persistante après reset.

Le firmware de banc ne lit ni ne réécrit la configuration iGate. Le
programme émetteur radio sert de pont entre des commandes USB `TX hex`
et les paquets du banc. Aucun service réseau n'est exposé.

## Trames expérimentales

Le préfixe `3c ff 7e` est utilisé uniquement sur ce banc isolé. Ce n'est
pas une allocation convenue pour le futur format commun APRS. Les
récepteurs APRS équipés du filtre de types inconnus le rejettent.

| Champ | Octets |
| --- | ---: |
| Préfixe et version `3c ff 7e 01` | 4 |
| Opération | 1 |
| Nonce aléatoire d'armement | 8 |
| Offset, entier gros-boutiste | 4 |
| Longueur du corps, gros-boutiste | 2 |
| Indicatif source, ASCII complété par zéros | 8 |
| Indicatif destinataire, même codage | 8 |
| Corps | 0 à 204 |
| HMAC-SHA256 tronqué | 16 |

Ces limites de huit caractères sont propres au banc, pas une proposition
pour le format natif. Source et destinataire sont présents dans chaque
trame, y compris dans les acquittements. Le firmware et les adresses ne
sont pas chiffrés.

Le HMAC couvre tous les octets avant le tag, précédés de la longueur
sur un octet et de l'identité du récepteur. Le récepteur est armé par
USB avec une clé aléatoire de 32 octets ; il génère alors un nouveau
nonce de huit octets. La clé reste en RAM, n'est pas affichée par le
firmware et disparaît au reset. Une copie privée à permissions 0600
est conservée par l'outil de test. Cet armement USB ne constitue pas
encore une procédure d'administration distante.

## Session

1. `START` transporte la longueur de l'image (4 octets), son SHA-256
   (32 octets) et l'identifiant de carte de laboratoire (4 octets).
   Le récepteur choisit la partition inactive ; le boot reste inchangé.
2. `DATA` transporte au plus 204 octets à l'offset indiqué. Un seul bloc
   est en attente d'acquittement. Les données hors ordre sont refusées.
   Une retransmission déjà écrite est comparée aux octets de la flash,
   puis acquittée sans seconde écriture.
3. `FINISH` n'est accepté qu'après réception de toute l'image. SHA-256
   est recalculé en relisant la flash, puis `esp_ota_end` valide l'image
   ESP32. Cette opération ne sélectionne pas encore le nouveau firmware.
4. `ACTIVATE`, signé séparément, sélectionne cette partition pour le
   démarrage puis redémarre la carte après l'acquittement.
5. `ABORT`, le désarmement USB ou l'expiration annulent la session.

L'acquittement signé contient l'opération d'origine, le statut et le
prochain offset attendu. L'émetteur retente jusqu'à huit fois en cas
d'absence d'acquittement. Une minute sans paquet authentifié pendant
un transfert ou après vérification annule la session. L'armement expire
aussi après 45 minutes.

Après un reset pendant le transfert, l'application précédente reste
sélectionnée ; la session doit être réarmée et recommencée. Ce prototype
**ne met pas en œuvre le rollback automatique d'une application déjà
activée qui démarrerait mal**. Il ne démontre pas non plus la récupération
d'un site après perte d'alimentation ou d'accès radio.

## Compilation et essais

Sources : [`tools/lora_ota/`](../../tools/lora_ota/). Trois variantes :
`bridge`, `receiver` (label `lab-a`) et `receiver_b` (label `lab-b`).

```sh
pio run -d tools/lora_ota -e bridge -e receiver -e receiver_b
c++ -std=c++17 -Wall -Wextra -Werror -Itools/lora_ota/include \
  tools/lora_ota/test_session.cpp -o /tmp/lora-ota-session-test
/tmp/lora-ota-session-test
```

Le transfert s'effectue avec `tools/lora_ota/send.py`. Ses paramètres
`--bridge`, `--receiver`, `--image` et `--log` sont obligatoires.
`--fault-checks` provoque un mauvais tag, un mauvais SHA-256, un reset
en cours de transfert, un bloc hors ordre et une perte d'acquittement.
`--activate` doit être ajouté pour sélectionner l'image vérifiée.
Les sauvegardes de flash, clés, journaux et configurations restent
dans le dossier local de l'essai sous `logs/`, jamais dans une publication.

Les résultats sur carte doivent être présentés séparément du protocole
et restent limités à la liaison, aux images et aux défauts effectivement
essayés.
