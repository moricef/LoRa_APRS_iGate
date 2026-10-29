# Transfert de firmware par LoRa entre deux ESP32

Essai du 29 octobre 2026, sur un banc autonome issu de la base iGate
`c28bd87`, branche locale `experiment/lora-ota`. Protocole et sources :
[banc OTA direct](../../proposals/LORA_OTA_BENCH_fr.md).

## Objectif et conditions

Vérifier qu'une image d'application ESP32 peut être reçue par LoRa,
écrite dans la partition inactive, vérifiée sans changer le programme
de démarrage, puis activée par une commande distincte.

- Deux TTGO LoRa32 V2.1, radios SX1278 : émetteur F4MLV-2, récepteur
  F4MLV-15. Liaison locale directe ; ce n'est pas un essai de portée.
- PlatformIO espressif32 6.12.0, Arduino-ESP32 2.0.17
  (`3.20017.241212+sha.dcc1105b`), RadioLib 7.6.0, esptool 4.9.0.
- Profil fixe : 434,500 MHz, BW500, SF7, CR 4/5, préambule 8, 2 dBm,
  CRC LoRa activé. Le débit nominal de modulation est 21 875 bit/s.
- Pont USB à 115 200 bit/s, commandes et trames représentées en hexadécimal.
- Un bloc en vol, au plus 204 octets utiles, acquittement de chaque bloc,
  HMAC-SHA256 tronqué à 16 octets et armement préalable par USB.
- Récepteur initial `lab-a` dans `app0`, nouvelle image `lab-b` destinée
  à `app1`. Partitions d'application de 1 966 080 octets chacune.

## Résultat du transfert complet

| Observation | Résultat |
| --- | --- |
| Image transmise | 324 112 octets |
| Blocs distincts | 1 589 |
| Début du transfert principal | 18:46:51 UTC |
| Fin et vérification de l'image | 18:52:18 UTC |
| Durée transfert + vérification | 326,72 s, environ 5 min 27 s |
| Débit utile observé, chaîne complète | 992 octets/s, environ 7,94 kbit/s |
| Relances du transfert principal | 4, dont une provoquée en ignorant un acquittement |
| État après vérification, avant activation | `lab-a`, running `app0`, boot `app0`, état READY |
| État après activation | `lab-b`, running `app1`, boot `app1` |

Les trois autres relances correspondent à une absence d'acquittement
constatée par l'émetteur ; leur cause n'est pas localisée. Elles ne sont
pas assimilées à trois erreurs RF mesurées.

Le récepteur a recalculé SHA-256 en relisant la partition écrite, puis
`esp_ota_end` a validé l'image. La lecture indépendante de `app1` par
esptool, après le démarrage de `lab-b`, restitue exactement les 324 112
octets de l'image envoyée :

```text
245442c255ea5cd3ab87b92c4804b521391319e9287966309e389b3c5e0ffca8
```

La commande FINISH n'a pas modifié la sélection de démarrage ; seul
ACTIVATE l'a fait. Le label et les partitions ont été observés par USB.

## Défauts provoqués

Les contrôles suivants ont été exécutés sur les cartes avant ou pendant
le transfert réussi :

- Un START dont le tag HMAC a été altéré n'a obtenu aucun acquittement
  authentifié et a laissé le récepteur en IDLE, offset zéro.
- Un transfert court dont le SHA-256 annoncé était faux a été refusé à
  FINISH ; l'image n'a pas été sélectionnée.
- Après un bloc d'une image incomplète, un redémarrage demandé par USB
  a redémarré `lab-a` dans `app0`, avec la session abandonnée. Un nouvel
  armement a fourni un nonce différent et le transfert a recommencé.
- FINISH avant réception de toute l'image a été refusé.
- Un bloc arrivant avant son offset attendu a été refusé.
- Un acquittement a été ignoré volontairement par le client. La
  retransmission du bloc puis son renvoi explicite ont été acquittés,
  sans avancer une deuxième fois l'offset d'écriture.

Les tests hôtes de la même machine d'état, compilés avec ASan et UBSan,
couvrent aussi une erreur d'écriture simulée et le refus d'une activation
avant vérification. Ces deux derniers points ne sont pas des pannes
physiquement provoquées sur les cartes.

## Portée de la conclusion du premier banc

Les limites ci-dessous concernent la première étape autonome ; la seconde
étape intégrée est décrite ensuite.

Ce banc démontre **le transfert radio d'une application complète, sa
vérification dans la flash et son démarrage sur l'autre partition** sur
ces deux cartes. Il ne démontre pas un OTA prêt à déployer à distance :

- L'image de 324 Ko est celle du banc, pas le firmware iGate complet.
- L'armement et la fourniture de la clé utilisent encore USB ; ce n'est
  pas une administration entièrement distante.
- Aucun changement de fréquence/profil, relais intermédiaire, patch
  différentiel ou reprise persistante après reset n'est testé.
- Le redémarrage provoqué n'est pas une coupure d'alimentation pendant
  une écriture flash.
- Le retour automatique après activation d'un firmware défectueux n'est
  pas validé. Le core Arduino utilisé confirme par défaut l'image OTA
  avant l'initialisation fonctionnelle de l'application : cette confirmation
  ne prouve pas que la radio ni le service attendu fonctionnent.
- La coexistence avec le trafic APRS habituel et les liaisons de sites
  éloignés ne sont pas évaluées.

Les images exactes, événements JSONL et résultat de la lecture flash sont
conservés localement sous `logs/lora-ota-20261029/` dans le worktree
principal. Les clés et sauvegardes de configuration ne sont pas publiables.

## Deuxième étape : firmware iGate complet et contrôle par radio

Cette étape reprend la même base matérielle et le même profil fixe. La
branche `experiment/lora-ota` inclut désormais le décodeur PPM autonome
ainsi que RXT et le format natif. Les variantes d'essai sont
`ttgo-lora32-v21-ota-a` et `ttgo-lora32-v21-ota-b`, versions
`4.0.2RXT-OTA-A` et `4.0.2RXT-OTA-B`.

Le récepteur exécute le vrai iGate dans `app0`. L'armement, le transfert,
l'activation et la confirmation sont commandés par radio authentifiée ;
le programme de transfert n'ouvre que l'USB de l'émetteur. La clé est
provisionnée avant l'essai. Aucun accès USB au récepteur n'intervient
pendant le transfert et les validations de démarrage.

| Observation | Résultat |
| --- | --- |
| Image iGate OTA-B | 1 743 216 octets |
| Blocs distincts | 8 545, au plus 204 octets chacun |
| Premier bloc acquitté | 19:46:42 UTC |
| Image complète vérifiée | 20:15:44 UTC |
| Intervalle entre ces deux observations | 1 741,71 s, environ 29 min 2 s |
| Débit utile sur cet intervalle | environ 1 001 octets/s, soit 8,01 kbit/s |
| Reprises après arrêt du client émetteur | 2, aux offsets 161 364 et 486 948, sans recommencer l'écriture |
| Relances DATA dans les trois segments | 193, dont trois acquittements ignorés volontairement |
| État après FINISH, avant activation | OTA-A ; application active et démarrage sélectionné dans `app0` |

Le débit inclut les interruptions du client et les reprises. Les relances
ne constituent pas un décompte d'erreurs radio : une absence d'acquittement
ne localise pas la perte, et trois sont provoquées. Le pont USB utilise
ici 921 600 bit/s ; le débit nominal radio reste 21 875 bit/s.

Le SHA-256 attendu, contrôlé en relisant la partition reçue, est :

```text
61ff21e9e2f04ef27c69f934c561c7d726b1a92cd30b9e85e1cc6beed6587797
```

La lecture indépendante de `app1` avec esptool, après confirmation,
restitue exactement les 1 743 216 octets de l'image envoyée. Cette lecture
redémarre la carte ; la vérification suivante porte donc aussi sur la
persistance de la sélection après reset.

### Contrôles intégrés

- FINISH avant réception complète et DATA hors ordre ont été refusés.
  Les retransmissions après perte d'acquittement n'ont pas écrit le même
  bloc une deuxième fois.
- L'accès HTTP `/ota/start?mode=firmware` pendant la session radio a reçu
  le statut 409 : une mise à jour HTTP concurrente est bloquée.
- Un défaut ponctuel, demandé par une commande de diagnostic authentifiée,
  provoque un reset au premier démarrage d'OTA-B, avant sa confirmation.
  Après ce défaut, la réponse radio identifie OTA-A dans `app0`, sans
  validation en attente : le retour à l'ancienne application est observé.
- La même image déjà reçue est ensuite réactivée, sans retransmission.
  La réponse radio identifie OTA-B dans `app1`, en attente de confirmation,
  avec le SHA-256 annoncé.

- La confirmation est volontairement omise pendant 190 secondes après
  observation du démarrage d'OTA-B. La réponse radio suivante identifie
  OTA-A dans `app0`, sans confirmation en attente : le retour automatique
  après expiration du délai est observé.
- Après une nouvelle activation de la même image, CONFIRM signé est
  accepté ; OTA-B reste dans `app1`, sans confirmation en attente. La
  même demande de confirmation est répétée et reste acceptée.

### Réception après mise à jour

Après la lecture flash et son reset, un nouvel échange radio retrouve
OTA-B dans `app1`, sans confirmation en attente. Les essais suivants
passent sur cette application :

- Répéter le HELLO actif restitue le même nonce ; réutiliser son compteur
  après fermeture de session ne fournit pas de réponse authentifiée.
- Une demande signée avec une mauvaise clé ne fournit pas de réponse
  authentifiée. Une DATA signée avec le nonce de la session précédente
  est ignorée ; l'offset d'écriture courant reste zéro.
- Trois trames sont émises par MLV-2, produites par les vrais codecs :
  texte, natif et PPM autonome. Les trois contenus attendus sont retrouvés
  dans la sortie TNC2 du récepteur.
- La trame native porte une mesure RXT synthétique à l'index du relais
  F4MLV-2. La sortie conserve l'association `F4MLV-2<--F4MLV-9`.
  Ce contrôle vérifie l'association, pas la justesse d'une mesure RF.

Les tests hôtes de la machine d'état passent sous ASan/UBSan ; les tests
Python couvrent la taille maximale et le décodage des réponses signées.
Les variantes intégrées A/B et la variante ordinaire sans OTA radio
compilent. Le hook de confirmation différée est fort dans A/B et conserve
le comportement faible d'origine dans la variante ordinaire.

### Limites de l'intégration

Cette étape valide un firmware iGate complet sur deux cartes proches,
avec profil fixé avant essai. Elle ne valide pas encore :

- Le basculement négocié entre le profil APRS et un profil de transfert,
  ni le rétablissement de ce profil après une interruption.
- Le transfert à travers des relais, les mises à jour différentielles et
  la reprise persistante après reset du récepteur.
- Une coupure d'alimentation pendant l'écriture ou des migrations de
  configuration incompatibles entre ancienne et nouvelle applications.
- Les autres modèles ESP32, les bootloaders déjà installés à distance
  et les identités APRS de neuf caractères : l'adresse de ce prototype
  est encore limitée à huit caractères.
- Le fonctionnement normal de tous les services pendant le transfert :
  les émissions APRS, capteurs et écran sont suspendus pendant la session.
  Le test de réception après mise à jour ne remplace pas un essai prolongé.

### Traces

Les journaux de cette étape sont conservés localement sous
`logs/lora-ota-integrated-20261029/`. Le transfert utilise les segments
`radio-update-validated.jsonl`, `radio-update-resumed.jsonl` et
`radio-update-final.jsonl` ; ils décrivent une même réception poursuivie
sur le récepteur. Les durées de segments ne doivent pas être prises pour
la durée du transfert complet.
