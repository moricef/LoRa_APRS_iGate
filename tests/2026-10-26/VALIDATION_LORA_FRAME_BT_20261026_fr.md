# Types de trames LoRa et Bluetooth inutilisé — 26 octobre 2026

> Compte rendu historique. Les conclusions portent sur les versions et les conditions décrites dans ce document. Pour les références actuelles, consulter l’[index documentaire](../../README.md).

## Résultats établis

Les builds RXT et PPM compilent avec un filtre qui rejette les types binaires
inconnus et sans l'appel à `btStop()`. Sur le variant ESP32 classique
`ttgo-lora32-v21_SD`, la suppression de cet appel réduit la flash d'environ
128 000 octets (125 Kio) et la RAM statique de 3 828 octets.

Ces changements sont locaux et non commités. Aucune carte n'a été flashée
pendant cette validation. Le gain de mémoire libre après démarrage et le
comportement RF des nouveaux binaires ne sont donc pas encore mesurés.

## Réception des types inconnus

Le garde-fou partagé `include/lora_frame_type.h` est appelé après les
décodeurs binaires reconnus et avant la conversion en texte :

- branche `rxt`, base `cbd80dd` : texte `3C FF 01` et natif `3C FF 02` ;
- branche locale `ppm-test`, worktree durable `../LoRa_APRS_iGate-ppm`,
  base `e82044a` : mêmes formats et compression autonome `3C FF 81` ;
- les autres types, notamment `C1`, sont rejetés avant la chaîne texte,
  JSON et relais ; le préfixe incomplet `3C FF` est rejeté également ;
- les paquets texte historiques sans préfixe restent acceptés ; le bloc
  RXT caché d'une trame texte n'est pas modifié par ce filtre.

Le test hôte couvre les 256 valeurs du type, avec et sans un texte TNC2
plausible derrière le préfixe. Il vérifie également les cas texte classique,
texte sans préfixe, extension après octet nul et préfixe incomplet.
Les décodeurs natif et PPM existants gardent leur place avant ce filtre.

Vérifications exécutées :

```sh
make -C test/lora_frame_type_host test
make -C test/native_aprs_host test
make -C test/rxt_host test
```

Résultats : tests de type réussis, 149 vérifications natives réussies,
tests hôtes RXT et télémétrie APRS réussis. Les deux branches ont passé
`git diff --check`.

## Comparaison Bluetooth

Chaque paire a été compilée avec le même filtre de trame et le même variant,
avant puis après retrait de l'appel `btStop()` dans `src/wifi_utils.cpp`.
Les outils de génération incorporent la date du build dans la WebUI ; les
écarts de flash ne constituent donc pas une mesure à l'octet près du seul
code Bluetooth.

| Build | Flash avec btStop | Flash sans btStop | Réduction | RAM statique avec / sans |
|---|---:|---:|---:|---:|
| RXT | 1 761 609 (89,6 %) | 1 633 393 (83,1 %) | 128 216 | 57 924 / 54 096 |
| PPM autonome | 1 891 257 (96,2 %) | 1 763 089 (89,7 %) | 128 168 | 59 972 / 56 144 |

Partition application : 1 966 080 octets. RAM utilisée indiquée par
PlatformIO : données statiques, pas mémoire libre pendant l'exécution.
Les quatre compilations se terminent avec succès.

Le cœur Arduino installé explique le résultat :

- `esp32-hal-bt.c` fournit un `btInUse()` fort qui retourne vrai sur ESP32 ;
- `esp32-hal-misc.c` fournit par défaut un `btInUse()` faible qui retourne faux ;
- `initArduino()` libère la mémoire du contrôleur inutilisé lorsque ce
  dernier retourne faux ;
- dans l'ELF RXT avec l'appel, `btInUse` est fort ; sans l'appel, il est
  faible. Le contrôleur Bluetooth n'est pas utilisé ailleurs dans notre
  code applicatif.

Ce constat explique pourquoi le retrait peut aussi améliorer le tas au
démarrage, mais aucun nombre de mémoire libre sur carte n'est déduit de
l'ELF ou du seul pourcentage RAM de PlatformIO.

## Traces locales

`logs/frame-bt-review-20261026/` contient les quatre journaux de compilation,
les binaires et les ELF avant/après. Les SHA-256 des binaires sont dans
`SHA256SUMS`. Les binaires de publication `docs/firmware/` n'ont pas été
remplacés. L'inventaire des firmwares déployés ne change pas.
