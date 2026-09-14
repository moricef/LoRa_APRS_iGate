# Web Flasher du fork

Le flasher publie uniquement le variant `ttgo-lora32-v21_SD` de la branche
`feature/rxt-integration`. Il réalise une installation complète et remplace
la configuration SPIFFS.

ESP Web Tools efface toute la flash par défaut lorsqu'il considère
l'opération comme une nouvelle installation. Ce firmware ne fournissant pas
de détection Improv Serial, un manifeste contenant seulement `firmware.bin`
ne constitue pas un mécanisme de mise à jour sûr : il peut laisser la carte
sans bootloader ni table de partitions. Les mises à jour conservant la
configuration doivent passer par ElegantOTA.

## Mettre les binaires à jour

```sh
pio run -e ttgo-lora32-v21_SD
pio run -e ttgo-lora32-v21_SD -t buildfs

cp .pio/build/ttgo-lora32-v21_SD/bootloader.bin docs/firmware/ttgo-lora32-v21_SD/
cp .pio/build/ttgo-lora32-v21_SD/partitions.bin docs/firmware/ttgo-lora32-v21_SD/
cp .pio/build/ttgo-lora32-v21_SD/firmware.bin docs/firmware/ttgo-lora32-v21_SD/
cp .pio/build/ttgo-lora32-v21_SD/spiffs.bin docs/firmware/ttgo-lora32-v21_SD/
cp ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin docs/firmware/ttgo-lora32-v21_SD/
```

Mettre également à jour la version et la date dans `index.html` et dans
`manifest-ttgo-lora32-v21-sd.json`.

Régénérer ensuite `firmware/ttgo-lora32-v21_SD/SHA256SUMS` avec :

```sh
cd docs/firmware/ttgo-lora32-v21_SD
sha256sum bootloader.bin partitions.bin boot_app0.bin firmware.bin spiffs.bin > SHA256SUMS
```

Le manifeste installe aussi `spiffs.bin`. La configuration présente dans la
mémoire SPIFFS est donc remplacée par la configuration par défaut du build.
L'utilisateur doit sauvegarder son JSON avant le flash et le restaurer ensuite.

## Publication

GitHub Pages doit publier la branche `gh-pages`, depuis sa racine. Copier le
contenu de `docs/` à la racine de cette branche après chaque mise à jour.
