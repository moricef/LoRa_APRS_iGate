# Web Flasher du fork

Le flasher publie trois variants de la branche `feature/per-destination-dedup` :

- `ttgo-lora32-v21_SD` (ESP32) ;
- `heltec_wifi_lora_32_V3_2` (ESP32-S3) ;
- `QRPLabs_LightGateway_Plus_1_0` (ESP32-S3).

Comme le flasher du Tracker, son manifeste contient toujours le bootloader,
la table de partitions, `boot_app0.bin` et `firmware.bin`, avec
`new_install_prompt_erase: true`. ESP Web Tools propose ainsi le choix :

- sans effacement, SPIFFS n'est pas écrit et la configuration est conservée ;
- avec effacement, toute la flash est nettoyée et l'iGate recrée
  `/igate_conf.json` avec ses valeurs par défaut au premier démarrage.

Ne pas ajouter `spiffs.bin` au manifeste : il écraserait la configuration
même lorsque l'effacement n'est pas demandé. Ne pas proposer non plus un
manifeste contenant seulement `firmware.bin` : si ESP Web Tools considère
l'opération comme une nouvelle installation, son effacement préalable
laisserait la carte sans bootloader ni table de partitions.

## Mettre les binaires à jour

```sh
pio run -e ttgo-lora32-v21_SD -e heltec_wifi_lora_32_V3_2 -e QRPLabs_LightGateway_Plus_1_0

cp .pio/build/ttgo-lora32-v21_SD/bootloader.bin docs/firmware/ttgo-lora32-v21_SD/
cp .pio/build/ttgo-lora32-v21_SD/partitions.bin docs/firmware/ttgo-lora32-v21_SD/
cp .pio/build/ttgo-lora32-v21_SD/firmware.bin docs/firmware/ttgo-lora32-v21_SD/
cp ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin docs/firmware/ttgo-lora32-v21_SD/
```

Répéter les quatre copies dans le répertoire propre à chaque variant. Le
bootloader ESP32 est flashé à `0x1000`, tandis que les bootloaders ESP32-S3
sont flashés à `0x0000`. Les autres offsets restent identiques : table de
partitions à `0x8000`, `boot_app0.bin` à `0xe000` et firmware à `0x10000`.

Mettre également à jour la version, la date et l'heure UTC exacte de chaque
build dans `index.html` et dans chacun des trois manifestes. Ces valeurs doivent
correspondre au champ `Build date` intégré dans la WebUI du binaire concerné.

Régénérer ensuite `firmware/ttgo-lora32-v21_SD/SHA256SUMS` avec :

```sh
cd docs/firmware/<variant>
sha256sum bootloader.bin partitions.bin boot_app0.bin firmware.bin > SHA256SUMS
```

## Publication

GitHub Pages doit publier la branche `gh-pages`, depuis sa racine. Copier le
contenu de `docs/` à la racine de cette branche après chaque mise à jour.
