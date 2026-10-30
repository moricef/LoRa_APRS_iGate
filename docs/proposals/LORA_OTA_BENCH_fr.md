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

## Profil rapide temporaire

L'armement et la confirmation utilisent le profil enregistré dans l'iGate.
La commande authentifiée PREPARE_FAST (11) demande le profil temporaire
numéro 1 et un bail d'inactivité : corps de trois octets, `01` suivi d'un
entier big-endian de 15 à 60 secondes. Les autres profils sont refusés.
Ce profil de banc est 434,500 MHz, BW500, SF7, CR4/5. Un profil SF5
éventuel devra avoir un autre identifiant, sans changer le sens du profil 1.
Il ne constitue pas
une allocation de fréquence ou un profil universel pour un réseau déployé.

L'acquittement part sur le profil initial. Deux secondes après la fin de
son émission, le récepteur passe au profil rapide ; le contrôleur change
son propre profil et vérifie le lien par une commande STATUS authentifiée.
Si l'acquittement initial manque, le contrôleur peut quand même essayer
STATUS sur le profil rapide. Une répétition de PREPARE_FAST pendant
l'attente ne repousse pas la date de changement.

Chaque commande authentifiée de la session renouvelle le bail rapide.
À son expiration, le récepteur abandonne le transfert et recharge les
paramètres radio de Config. La configuration n'est pas réécrite lors
d'un changement temporaire. Un reset redémarre donc directement sur le
profil configuré, même si le PC ou la radio de contrôle ne répondent plus.

RETURN_NORMAL (12), sans corps, est acquitté sur le profil rapide ; le
retour intervient 500 ms après cet acquittement. La session reste active,
ce qui permet l'activation après réception complète et sa confirmation
sur le profil initial. Le client vérifie ce retour par STATUS. Une perte
de cet acquittement n'empêche pas de retrouver le récepteur sur le profil
initial. ABORT et l'expiration globale de session rétablissent aussi Config.

Le client `send_integrated.py --negotiate-profile` effectue ce cycle. Le
pont possède deux commandes locales, PROFILE 0 et PROFILE 1. Pour les
seuls essais décrits ici, PROFILE 0 est 434,100 MHz BW125 SF12 CR4/5 ;
le récepteur utilise les mêmes paramètres provisoires enregistrés avant
l'essai. Après l'essai, les configurations précédentes sont remises.
Cette convention du pont devra être remplacée par une configuration
explicite pour un usage sur d'autres profils réseau.

La commande TEST_REBOOT (13) est réservée aux variantes de diagnostic :
elle provoque un reset sans sélectionner l'image en cours de réception.
`check_profiles.py` l'emploie pour vérifier le retour au profil initial.
Le bail et les délais sont testés séparément sur hôte, y compris au
rebouclage du compteur millis. La reprise persistante après reset reste
hors de ce premier mode d'envoi bloc par bloc. Le mode groupé décrit
ci-dessous ajoute cette fonction.

Les TTGO du banc portent un SX1278. BW500/SF7/CR4/5 donne un débit nominal
de 21 875 bit/s. Le profil BW500/SF5/CR4/5 proposé par Jon donne 62 500
bit/s mais nécessite une radio prenant en charge SF5, comme le SX1262 ;
il n'est pas validé par cet essai matériel.

## Groupes de blocs, cache du relais et reprise persistante

Cette extension expérimentale conserve le mode précédent. Elle est activée
par `send_integrated.py --windowed --via INDICATIF_RELAIS` ; les trois
équipements doivent porter un firmware compatible. Les validations sur
ordinateur et les résultats sur cartes sont distingués dans les comptes
rendus d'essai.

Une page de flash de 4 096 octets forme un groupe de 32 blocs de 128 octets.
La dernière page peut être plus courte. Le contrôleur transmet les blocs
au relais sans attendre d'acquittement individuel. Le relais les conserve
en RAM et reste en réception pendant cette phase. À la demande de bilan,
il transmet les blocs à la cible, puis lui demande son bitmap de réception.
Cette alternance évite que le contrôleur et le relais émettent le groupe
simultanément avec leurs radios uniques.

| Opération | Corps | Effet |
|---|---|---|
| WINDOW_START (15) | Taille, SHA-256 et carte, comme START | Ouvre ou reprend la même image et indique la progression durable |
| WINDOW_DATA (16) | 1 à 128 octets, offset multiple de 128 | Remplit le groupe courant, sans ACK par bloc |
| WINDOW_QUERY (17) | Masque attendu de 32 bits | Renvoie les blocs présents et la progression durable |
| WINDOW_FINISH (18) | Vide | Vérifie l'image complète, sans la sélectionner au démarrage |
| WINDOW_ACTIVATE (19) | Vide | Sélectionne séparément l'image vérifiée |

Les réponses de WINDOW_START et WINDOW_QUERY contiennent l'opération,
le statut, l'offset durable (4 octets) et le bitmap (4 octets), tous couverts
par le HMAC de la cible. Les requêtes et réponses utilisent les mêmes
adresses, nonce, compteur d'armement et authentification que l'OTA v2.
L'enveloppe de relayage conserve les trames signées de bout en bout.

Le relais répare les trous à partir des trames signées gardées en cache,
avec trois tours locaux au maximum avant de remettre le bilan au contrôleur.
Le contrôleur renvoie alors seulement les blocs qui manquent encore.
Le cache contient un groupe, reste en RAM et disparaît après un reset ;
il ne stocke pas une image complète. Les bitmaps ne sont pas persistants.

Sur la cible, les blocs peuvent arriver dans le désordre au sein du groupe.
Une page complète est écrite dans la partition inactive, puis un journal
atomique NVS enregistre la taille, le SHA-256, la partition et le nouvel
offset durable. Une coupure avant ce dernier enregistrement fait retransmettre
la page entière. Les pages déjà enregistrées ne sont pas effacées à la reprise.
Le journal est écrit par page, pas par bloc. Un autre manifeste est refusé
tant que le contrôleur n'a pas explicitement annulé l'image en cours.

Après une coupure, la nouvelle session doit être réauthentifiée. Le client
attend le retour autonome aux profils initiaux, réarme la cible, repasse
au profil rapide et demande son offset durable. Les tentatives automatiques
de récupération sont bornées. Un redémarrage du client peut aussi reprendre
à partir du journal de la cible avec le même fichier, sans journal de session
précédent. Un ABORT explicite efface la reprise ; une expiration de session
la conserve.

WINDOW_FINISH recalcule SHA-256 en relisant la partition et vérifie le format
de l'image ESP32. WINDOW_ACTIVATE vérifie à nouveau l'image avant sélection.
La confirmation radio et le retour arrière du firmware restent ceux du mode
précédent. Une progression persistante ne rend jamais une image incomplète
démarrable.

Les diagnostics de banc ajoutent une perte ponctuelle sur la cible
(opération 20, masque de 32 bits). `--fault-checks` provoque une perte sur
chaque liaison ; `--exercise-resume` provoque un reset après deux pages
durables et une demi-page volatile. Ces diagnostics n'établissent pas à eux
seuls la résistance aux coupures électriques pendant une écriture réelle.

Les chemins alternatifs, plusieurs relais successifs, plusieurs cibles,
les profils différents par liaison et SF5 restent hors de cette extension.
