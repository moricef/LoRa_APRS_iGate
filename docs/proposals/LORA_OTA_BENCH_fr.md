# Banc OTA LoRa direct — prototype du 29 octobre 2026

Deux étapes sont conservées dans ce document : le banc autonome v1,
décrit d'abord, puis [l'intégration v2 dans l'iGate](#intégration-v2-dans-ligate).
Les deux protocoles expérimentaux ne sont pas interchangeables.

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

## Intégration v2 dans l'iGate

Les variantes `ttgo-lora32-v21-ota-a` et `-ota-b` sont des firmwares iGate
complets, comprenant RXT, le format natif et le décodeur PPM autonome.
`ENABLE_RF_OTA` y active l'OTA radio ; les autres variantes n'activent
pas ce service. Ces variantes sont expérimentales et ne sont pas
publiées sur le flasher.

### Armement et authentification

Le préfixe devient `3c ff 7e 02`. Les champs binaires restent ceux du
banc, avec une source et un destinataire de huit octets. Le destinataire
est le champ **Callsign-SSID** de l'équipement ; la source est le
contrôleur authentifié configuré. Ces identités sont indépendantes du
nom tactique utilisé par les balises APRS. La limite expérimentale de
huit caractères exclut encore certaines identités APRS de neuf caractères ;
elle devra être levée avant un usage général sur les stations de Jon.

La clé est celle de REMOTE_AUTH, déjà provisionnée. Le HMAC-SHA256
tronqué à 16 octets couvre la chaîne ASCII `LORA-APRS-OTA2`, suivie de
tous les octets de la trame avant le tag. Ce domaine est distinct de
celui des commandes RC1 et du banc v1. La clé n'est pas transmise sur
la radio et n'est pas compilée dans l'image de firmware.

L'opération HELLO porte un nonce nul, un compteur strictement croissant
dans le champ offset et un corps vide. Le compteur est enregistré en
NVS avant acceptation. Le récepteur répond avec un nonce aléatoire de
huit octets, que les échanges suivants doivent porter. Une répétition
du HELLO accepté pendant la même session restitue le nonce sans
recommencer le transfert ; après fermeture ou redémarrage, le compteur
ancien est rejeté. Un nouveau HELLO est nécessaire après chaque boot.

L'outil `send_integrated.py` n'ouvre que le port USB du pont émetteur.
Il conserve le compteur de contrôle sur le PC. L'initialisation des
clés reste une opération préalable ; le transfert lui-même ne demande
ni commande USB, ni action HTTP sur le récepteur.

### Validation du démarrage

Le hook Arduino `verifyRollbackLater` est remplacé dans ces variantes
pour empêcher la validation automatique trop précoce. Une image en
état PENDING_VERIFY n'est confirmée qu'après réception de CONFIRM,
authentifié, portant son SHA-256 attendu. Le firmware relit et hache
sa partition avant `esp_ota_mark_app_valid_cancel_rollback`.

Un watchdog de 60 secondes protège l'exécution du démarrage en attente.
La boucle de service attend la confirmation radio au plus 180 secondes
depuis le début du setup, puis demande le rollback. La preuve apportée
par CONFIRM est le fonctionnement de la boucle et des deux sens radio,
ainsi que la concordance de l'image ; ce n'est pas un autotest exhaustif
des capteurs, décodeurs et services de l'iGate.

Cela suppose un bootloader et une disposition des partitions qui
prennent en charge le retour à une application précédente. Le banc
amorce cette disposition en USB. Aucun changement de bootloader ou
de table de partitions n'est effectué par le transfert radio.

### Opérations supplémentaires et acquittements

| Code | Opération | Fonction |
| --- | --- | --- |
| 6 | HELLO | Armement radio et nouveau nonce |
| 7 | CONFIRM | Validation radio du nouveau démarrage ; corps SHA-256 de 32 octets |
| 8 | STATUS | État, version et partition courante |
| 9 | TEST_FAIL_ONCE | Diagnostic uniquement : provoque un reset au prochain boot non confirmé |
| 10 | REACTIVATE | Rehash et resélection de la dernière image inactive vérifiée, sans retransmission |

START, DATA, FINISH, ACTIVATE et ABORT conservent leur code de v1.
REACTIVATE ne choisit ni une adresse arbitraire, ni une image inconnue :
la partition inactive, sa longueur et son SHA doivent correspondre au
manifeste enregistré lors de l'activation précédente. L'API ESP32
vérifie aussi l'image avant de la sélectionner.

Le corps d'un ACK DATA contient l'opération, le statut et l'offset
suivant, soit six octets. Pour les autres ACK, il ajoute le drapeau
pending, les adresses de partition active et sélectionnée, la longueur
et le texte de version, puis le SHA-256 du manifeste associé à l'image
active, ou des zéros si aucun manifeste ne lui est associé. Le statut
ne constitue pas une attestation de sécurité du code exécuté.

### Transfert et coexistence

Pendant une session, les émissions APRS, le traitement de relais,
l'écran et les capteurs sont suspendus ; la radio et le réseau restent
actifs. Les GET d'observation HTTP restent disponibles, les POST et
les routes `/ota/*` sont refusés en 409 pour éviter une seconde écriture
ou une reconfiguration pendant le transfert. Une fermeture de session
ou son expiration rétablit le traitement normal.

Le profil reste fixé pour l'essai à 434,500 MHz BW500 SF7 CR4/5. Aucun
basculement négocié vers un profil rapide, relayage multibond ou patch
différentiel n'est implémenté.

L'émetteur peut reprendre une session encore active avec `--resume-log` :
il retrouve son nonce, vérifie l'identité de l'image et demande l'offset
au récepteur. START identique est idempotent et n'efface pas le début
déjà reçu. Un redémarrage du récepteur annule encore la session ; la
reprise persistante après ce redémarrage n'est pas implémentée.

Les commandes USB de `RFOTA_DIAGNOSTICS` servent uniquement à préparer
le banc et n'ont pas vocation à être activées sur les sites distants.
Le code 9 est également réservé à ces variantes de diagnostic. Les
tests de l'image complète et du retour arrière sont consignés dans
le compte rendu, indépendamment de cette description du protocole.

### Reproduire l'essai intégré

```sh
pio run -e ttgo-lora32-v21-ota-a -e ttgo-lora32-v21-ota-b
pio run -d tools/lora_ota -e bridge_fast
python3 tools/lora_ota/test_integrated.py
python3 tools/lora_ota/send_integrated.py \
  --bridge /dev/serial/by-id/PORT_EMETTEUR \
  --key-file CHEMIN_PRIVE/secret.base64 \
  --counter-file CHEMIN_PRIVE/compteur \
  --image .pio/build/ttgo-lora32-v21-ota-b/firmware.bin \
  --log CHEMIN_PRIVE/transfert.jsonl \
  --fault-checks --exercise-rollback --exercise-confirm-timeout
```

Ces commandes supposent le pont et OTA-A préalablement installés, le même
profil radio aux deux extrémités et une clé/configuration de contrôleur
cohérente. Les options `--exercise-*` sont des essais destructifs de session
réservés au banc : elles provoquent des redémarrages et un retour arrière.
L'observation après mise à jour est effectuée par `check_integrated.py`,
avec des trames produites par les vrais codecs (`regression_frames.cpp`).
