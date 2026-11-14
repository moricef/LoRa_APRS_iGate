# Cycle OTA complet par un relais : essai du 14 novembre 2026

[English version](FULL_RELAYED_OTA_CYCLE_VALIDATION_20261114_en.md)

## Objet et montage

Vérifier sur cartes réelles le transfert d'une application entière, son
activation, le rollback sans confirmation, puis la réactivation et la
confirmation radio. Ce passage complète le transfert du 13 novembre sans
activation ; il utilise le client Python, pas une interface graphique.

Trajet : **F4MLV-2 -> F4MLV-10 -> F4MLV-15**, trois TTGO LoRa32 V2.1/SX1278.
Le contrôleur utilise le pont USB intégré au firmware iGate OTA unifié ;
le relais et la cible utilisent le même firmware, avec des rôles configurés.
Le port USB de la cible n'est pas ouvert par le client. Wi-Fi et AP de la
cible sont désactivés, état vérifié par radio avant et après la mise à jour.

| Paramètre | Contrôle | Transfert temporaire |
|---|---|---|
| Fréquence | 434,100 MHz | 434,500 MHz |
| LoRa | BW125 / SF12 / CR4/5 | BW500 / SF7 / CR4/5 |
| Puissance | 20 dBm | 9 dBm |

Image non compressée, blocs utiles de **160 octets**, pages de 4 096 octets.
Le présent essai valide le cycle à 160 octets, pas une taille optimale.

La cascade laissée sur F4MLV-15 comporte deux résistances de 820 ohms et
quatre de 51 ohms. L'insertion mesurée au NanoVNA est de **53 dB**, contre
**28 dB d'écart observé sur la liaison radio** lors de la caractérisation du
8 novembre. Ces deux valeurs ne sont pas interchangeables.

## Versions et résultat

- Départ : `4.0.2RXT-OTA`, build 11 novembre 2026, 15:00:28 UTC.
- Candidate : `4.0.2RXT-OTA-B`, variante sans diagnostics
  `ttgo-lora32-v21-ota-candidate-b`.
- Image : **1 782 112 octets**, SHA-256
  `a0011d32ee6aa92a70b1185707fd14886adad032bceea24bf748f30e3f925517`.
- Transfert et vérification : **3 762,66 s**, soit **environ 474 octets/s**
  utiles, échanges et réparations compris.
- Réparations : **173 blocs demandés** sur 165 événements `repair_page`.
- Nouvelles tentatives : **17 expirations de requêtes**, pas 17 pages renvoyées.
- Aucune perte ni reprise de session ; code de sortie **0**.
- Cycle entier : **4 264,53 s**, du choix de la route au dernier nettoyage.

La cible termine sur B, partition active et de démarrage `0x1f0000`,
`pending=false`, SHA-256 attendu relu dans une nouvelle session.

## Mise en oeuvre

Le client testé venait de la base `834bf7c` avec des changements locaux :
démarrage/fermeture du pont intégré, remplacement explicite de l'ancienne
candidate et relecture finale. Ils sont désormais conservés dans le commit
**`6a9dc46072d35bbe11a68d4c1d53b515b3668135`**, qui ajoute aussi le choix
explicite du modèle cible ; la valeur TTGO utilisée ici est inchangée.
Ce commit conserve le client, pas une reconstruction identique du binaire :
utiliser l'image archivée et son empreinte, pas une recompilation ultérieure.

Prérequis : Python >= 3.10 avec PySerial ; firmware OTA et disposition
bootloader/partitions compatibles préinstallés ; clé de commande cible et
contrôleur autorisé ; clé de route commune déjà installée dans relais/cible
et disponible sur le PC. Ne pas générer de nouvelles clés pour une route
déjà provisionnée. Conserver un compteur persistant, sans réinitialiser
l'anti-rejeu.

Les rôles se configurent par commandes APRS authentifiées :
`OTA=RELAY,F4MLV-15` vers le relais, `OTA=VIA,F4MLV-10` vers la cible.
Relire avec `OTA=ROUTE?`. Les deux cartes doivent écouter le profil de
contrôle avant lancement ; le relais ne balise pas pendant le transfert.
Fermer les moniteurs série du contrôleur et maintenir la cascade.

Contenu de `profiles.json` :

```json
{
  "normal": {"frequency_hz":434100000,"bandwidth_hz":125000,"spreading_factor":12,"coding_rate":5,"power_dbm":20},
  "transfer": {"frequency_hz":434500000,"bandwidth_hz":500000,"spreading_factor":7,"coding_rate":5,"power_dbm":9}
}
```

Commande équivalente au lancement historique, avec chemins adaptés à la
racine du dépôt et fichiers privés fournis par l'opérateur.
**Elle active le firmware et détruit préalablement le point de reprise de
l'ancienne candidate inactive.** Elle conserve les 160 octets historiques.

```bash
cd /path/to/LoRa_APRS_iGate
ROOT="$PWD"
BRIDGE=/dev/serial/by-id/CONTROLLER_USB_PORT
IMAGE="$ROOT/logs/ota-full-cycle-prepared-20261114/firmware-4.0.2RXT-OTA-B.bin"
TARGET_KEY="$ROOT/private/target-key.base64"
ROUTE_KEY="$ROOT/private/route-key.base64"
COUNTER="$ROOT/private/ota-counter"
PROFILES="$ROOT/private/profiles.json"
RUN="$ROOT/logs/ota-full-cycle-$(date -u +%Y%m%d-%H%M%SZ)"
mkdir -p "$RUN"
sha256sum "$IMAGE" > "$RUN/image.sha256"
python3 tools/lora_ota/send_integrated.py \
  --bridge "$BRIDGE" --source F4MLV-2 --target F4MLV-15 --via F4MLV-10 \
  --key-file "$TARGET_KEY" --route-key-file "$ROUTE_KEY" \
  --counter-file "$COUNTER" --board-id 0x54474732 \
  --negotiate-profile --radio-profiles "$PROFILES" \
  --windowed --block-size 160 --require-offline --replace-existing \
  --exercise-confirm-timeout --image "$IMAGE" --log "$RUN/transfer.jsonl" \
  > "$RUN/console.log" 2>&1
rc=$?
printf '%s\n' "$rc" > "$RUN/exit-code"
```

L'absence de `--compress` conserve l'image entière non compressée.
`--exercise-confirm-timeout` retient la confirmation 190 s après observation
du démarrage pour dépasser le délai firmware de 180 s. Le client vérifie le
rollback, réactive B, confirme deux fois (idempotence), puis relit
version/partition/SHA/`pending` avec un nouveau HELLO.

## Preuves et limites

Chronologie UTC, numéros de ligne de `transfer.jsonl` :

| Heure | Ligne | Événement |
|---|---:|---|
| 10:15:26,227 | 8 | Ancienne candidate inactive supprimée |
| 10:15:57,577 | 14 | Cible hors ligne confirmée |
| 10:16:18,382 | 19 | Profil rapide confirmé, bail 60 s |
| 11:19:01,343 | 24852 | Image entière vérifiée, statut 0 |
| 11:19:19,741 | 24857 | Retour au profil de contrôle confirmé |
| 11:20:03,475 | 24864 | B sur `0x1f0000`, `pending=true` |
| 11:23:30,836 | 24869 | Rollback vers version initiale sur `0x10000` |
| 11:24:15,901 | 24875 | Deuxième démarrage B, `pending=true` |
| 11:24:59,369 | 24880 | B confirmée, `pending=false` |
| 11:25:13,385 | 24883 | Cible toujours hors ligne |
| 11:25:48,706 | 24889 | Nouvelle session : version, partition et SHA attendus |

Journaux privés : `logs/ota-full-cycle-20261114-101435Z/`.
Le fichier de profils historique est sous
`logs/block-cascade-20261108/profiles-transfer-9dbm.json`.
Empreintes SHA-256 :

| Pièce | Empreinte |
|---|---|
| `transfer.jsonl` | `0e23615d0d0710b8dbfd9aaf48cc49104772702388b6ff341ad16eb0ba0bd329` |
| `console.log` | `b141a6d4643df3510f93feff9b751a435a1d410f6206cc22d0aaef9c53d735b0` |
| Fichier de profils | `9696fb33dd72c5f2f4d58eaeefb64ea8540b8325923ee35e73c6c69add6d7410` |

Le journal observe le résultat du délai de confirmation, pas l'instant exact
du rollback. Il contient les réponses authentifiées décodées du contrôleur,
pas tous les octets RF ni la console cible : il ne permet ni de recalculer
chaque HMAC indépendamment ni de localiser toutes les pertes.

Un passage de laboratoire, un relais et une cible : pas de validation terrain,
de coupure d'alimentation pendant l'activation, ni de preuve matérielle pour
les ports Heltec et QRP Labs. Les clés et sauvegardes privées ne font pas
partie des pièces publiables.
