# Premier transfert de firmware par LoRa entre deux ESP32

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

## Portée de la conclusion

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
