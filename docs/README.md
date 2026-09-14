# Web Flasher du fork

Le flasher publie uniquement le variant `ttgo-lora32-v21_SD` de la branche
`feature/rxt-integration`.

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
pio run -e ttgo-lora32-v21_SD

cp .pio/build/ttgo-lora32-v21_SD/bootloader.bin docs/firmware/ttgo-lora32-v21_SD/
cp .pio/build/ttgo-lora32-v21_SD/partitions.bin docs/firmware/ttgo-lora32-v21_SD/
cp .pio/build/ttgo-lora32-v21_SD/firmware.bin docs/firmware/ttgo-lora32-v21_SD/
cp ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin docs/firmware/ttgo-lora32-v21_SD/
```

Mettre également à jour la version et la date dans `index.html` et dans
`manifest-ttgo-lora32-v21-sd.json`.

Régénérer ensuite `firmware/ttgo-lora32-v21_SD/SHA256SUMS` avec :

```sh
cd docs/firmware/ttgo-lora32-v21_SD
sha256sum bootloader.bin partitions.bin boot_app0.bin firmware.bin > SHA256SUMS
```

## Publication

GitHub Pages doit publier la branche `gh-pages`, depuis sa racine. Copier le
contenu de `docs/` à la racine de cette branche après chaque mise à jour.
