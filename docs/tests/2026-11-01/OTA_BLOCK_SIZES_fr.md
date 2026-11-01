# Blocs OTA variables : essai matériel du 1 novembre 2026

[English](OTA_BLOCK_SIZES.md)

Les cinq tailles essayées ont livré le même échantillon de **8 192 octets**,
avec vérification SHA-256 du préfixe relu dans la partition inactive.
Cet essai valide le fonctionnement du banc en liaison directe. Il ne
détermine pas la taille optimale sur une liaison éloignée ou relayée.

## Versions et montage

- Branche `experiment/lora-ota`, commit `d2349cc` ; cible
  `ttgo-lora32-v21-ota-a`, pont `bridge_fast` et client `block_bench.py`.
- Deux TTGO LoRa32 V2.1 avec SX1278 : MLV-2 comme pont USB/radio,
  MLV-15 comme cible. Aucun relais intermédiaire.
- Profil normal : 434,100 MHz, BW125, SF12, CR4/5, 2 dBm.
- Profil de transfert : 434,550 MHz, BW250, SF8, CR4/6, 3 dBm.
- Wi-Fi et point d’accès de la cible désactivés, état vérifié par radio.
- Antennes et disposition conservées pendant la série ; distance et
  atténuation non mesurées. Aucune atténuation contrôlée ni perte injectée.

Un passage par taille, ordre mélangé avec la graine `20261101` :
128, 175, 96, 204, puis 32 octets. Chaque passage repart de zéro.
Les pages flash restent de 4 096 octets pour toutes les tailles.

SHA-256 de l’échantillon commun :
`e0a770d1726664bb14211e7f51e9aaef3fa39dfa5f6d151dc3d7bffda5030492`.

SHA-256 du binaire exécuté par la cible :
`9582c8cbf85a939d0efcfbdb95933647040d41b90ee9593d5a3948461406dd63`.
L’empreinte ELF annoncée par radio correspond à celle du binaire conservé :
`a01331388424317447a7005f59dbd8b2b78357ae88dba8a58d8a8086180e50b7`.

## Observations

| Bloc utile (octets) | Bitmap (octets) | Durée (s) | Temps d’antenne cumulé calculé (s) | Blocs retransmis | SHA-256 |
|---:|---:|---:|---:|---:|---|
| 32 | 16 | 45,84 | 41,70 | 1 | Conforme |
| 96 | 8 | 25,28 | 23,56 | 1 | Conforme |
| 128 | 4 | 22,02 | 20,63 | 0 | Conforme |
| 175 | 4 | 19,89 | 18,70 | 0 | Conforme |
| 204 | 4 | 19,39 | 18,28 | 0 | Conforme |

La durée commence avant WINDOW_START et se termine après la vérification
en flash. La négociation initiale, le retour au profil normal et la fermeture
de session sont hors de cet intervalle. Le temps d’antenne additionne les
émissions confirmées du pont et de la cible, avec leurs acquittements et
réparations. Il est calculé à partir des tailles et paramètres radio,
et non mesuré par un analyseur RF. Les lectures des compteurs sont exclues.

Les cinq passages ont conservé le même identifiant de démarrage de la
cible et ne signalent aucune erreur d’émission du pilote.

- Les blocs de 96, 175 et 204 octets exercent des fins de page plus courtes,
  sans faire traverser une limite de page à un bloc.
- Les blocs de 204 octets produisent des trames de 255 octets, maximum
  accepté en direct.
- Le passage de 96 octets a réparé le bloc d’indice 21 de la première page.
- Celui de 32 octets a réparé le bloc d’indice 77 : le bitmap de seize
  octets a donc servi à demander une réparation au-delà des 32 premiers
  blocs. L’origine RF de ces deux pertes n’est pas déterminée.

Le vérificateur d’échantillon n’a ni validé une image complète ni demandé
son activation. Chaque succès porte uniquement sur les 8 192 octets écrits
et relus, soit deux pages flash.

## Portée et suite

Le banc peut maintenant servir aux séries comparatives décrites dans le
[protocole d’évaluation](../../proposals/LORA_OTA_BLOCK_BENCH_fr.md).
Les durées ci-dessus décrivent cinq passages isolés, pas une distribution
statistique. Elles ne justifient pas un changement du bloc par défaut.

Restent à comparer les tailles avec répétitions, sur des conditions RF
documentées, puis avec le relais intermédiaire. Les bitmaps de douze octets,
les reprises après panne avec blocs variables et une image complète dans
cette nouvelle géométrie ne sont pas validés par cet essai.

Les traces, résultats CSV/JSON, binaires et empreintes sont conservés dans
`logs/block-smoke-20261101/`, dossier local ignoré par Git.
