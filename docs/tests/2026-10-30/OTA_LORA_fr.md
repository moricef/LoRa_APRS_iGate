# OTA LoRa avec la cible sans Wi-Fi, en direct puis par un relais — 30 octobre 2026

## Objectif et montage

Vérifier le transfert d'un firmware iGate complet, son activation et le
retour arrière sans connexion IP sur la cible. F4MLV-2 sert de pont USB/radio ;
F4MLV-15 reçoit le firmware par radio. Aucun relais intermédiaire ne participe
à cet essai.

Les deux cartes sont des TTGO LoRa32 V2.1, ESP32/SX1278. Le profil initial
de laboratoire est 434,100 MHz, BW125, SF12, CR4/5, puissance 2 dBm.
Le transfert utilise temporairement 434,500 MHz, BW500, SF7, CR4/5, 2 dBm,
après négociation radio. Ces paramètres ne sont pas ceux du réseau en service.

Version expérimentale : branche `experiment/lora-ota`, base `34526b1`, avec
les modifications locales ajoutant la commande authentifiée NETWORK_STATUS
et le diagnostic de désactivation du Wi-Fi. Les images figées de cet essai
ne contiennent pas le transport multirelais développé ensuite.

## Résultats

| Mesure | Observation |
|---|---|
| Image OTA-B | 1 744 480 octets, 8 552 blocs |
| Transfert des données et vérification | 26 min 38 s |
| Débit utile sur cette durée | 1 092 octets/s environ |
| Retransmissions DATA | 137, dont une provoquée par l'ignorance volontaire d'un ACK |
| Intégrité | SHA-256 validé sur la cible ; lecture indépendante du slot app1 identique au fichier envoyé |
| Wi-Fi de la cible | Mode OFF, aucune connexion ni interface réseau active, contrôlés par réponse radio authentifiée avant et après la mise à jour |

SHA-256 de l'image reçue :
`e6279b9f3868b75de7daedd6b0d717e0b900ec901ceb5df6411e0b1ca4332ce6`.

Après transfert, un redémarrage provoqué avant confirmation du nouveau
firmware a ramené la cible sur OTA-A. Une autre activation sans confirmation
pendant le délai prévu a également ramené la cible sur OTA-A. OTA-B a ensuite
été activé et confirmé par radio. Un redémarrage supplémentaire a conservé
OTA-B, sans Wi-Fi actif.

## Portée de la conclusion

Le transfert, les commandes d'activation et la confirmation sont passés
par radio. La préparation initiale de la cible et la lecture indépendante
de la flash utilisent l'USB ; l'ordinateur contrôleur est relié en USB au
pont radio. La durée de 26 min 38 s exclut la préparation, les essais de
retour arrière, l'activation et la confirmation.

Cet essai valide une liaison directe de laboratoire. Il ne valide ni le
passage par un relais intermédiaire, ni une reprise persistante du transfert
après reset, ni le fonctionnement sur un site distant ou en réseau chargé.
Les retransmissions comptées ne permettent pas à elles seules d'identifier
la cause des pertes.

Les preuves sont conservées dans le dossier privé
`logs/lora-ota-offline-20261030/` : journal radio, synthèse, comparaison de
lecture flash et contrôle radio après redémarrage. Les clés et configurations
de ce dossier ne sont pas destinées à la publication.

## Prévol avec un relais intermédiaire

Un second montage utilise F4MLV-2 → F4MLV-10 → F4MLV-15. Le Wi-Fi de
F4MLV-10 reste actif pour la gestion ; celui de la cible reste coupé.
Les firmwares expérimentaux ajoutent une enveloppe authentifiée par saut,
qui conserve intacte l'authentification de la commande destinée à la cible.
Chaque enveloppe porte l'indicatif de la station qui la transmet.

Avant tout transfert d'image, le script `tools/lora_ota/check_route.py`
a observé :

- aucune réponse à un HELLO direct pendant les 12 secondes d'écoute ;
- aucune réponse au HELLO encapsulé lorsque le relais est suspendu,
  également pendant 12 secondes ;
- une réponse authentifiée de la cible après réactivation du relais ;
- une réponse radio indiquant Wi-Fi OFF et aucune interface réseau active ;
- le passage au profil rapide, puis le retour au profil initial ;
- sept requêtes et sept réponses retransmises par F4MLV-10, sans erreur
  d'émission déclarée, puis fin de session au profil initial.

Le script s'est terminé avec le code zéro. Ces contrôles valident les
échanges de commande du montage et le changement de profil.
Les preuves sont dans `logs/lora-ota-multihop-20261030/preflight.jsonl`.

## Transfert complet par F4MLV-10

Le transfert suivant, effectué avec le même montage et les images intégrant
le transport par relais, s'est terminé avec le code zéro.

| Mesure | Observation |
|---|---|
| Image OTA-B | 1 760 528 octets, 10 061 blocs de 175 octets au maximum |
| Transfert des données et vérification | 1 h 08 min 18 s |
| Débit utile sur cette durée | 430 octets/s environ |
| Retransmissions DATA | 290, dont une provoquée par l'ignorance volontaire d'un ACK |
| Réseau de la cible | Wi-Fi OFF et aucune interface active avant et après mise à jour, contrôlés par radio |
| Nouveau firmware | OTA-B dans app1, confirmé deux fois par radio ; aucune confirmation encore attendue |
| Lecture indépendante | Partition app1 lue par USB, identique octet pour octet au fichier envoyé |

SHA-256 de l'image envoyée :
`f8565e06ba8d170197573fb65931a7aba81d4c7d28150b8e64be913b8b9d1f76`.

La cible a vérifié l'image complète avant son activation. Deux retours
arrière ont été observés : après un reset provoqué avant confirmation,
puis après une activation laissée sans confirmation au-delà du délai.
Dans les deux cas, OTA-A a répondu par le relais. La même image OTA-B a
ensuite été réactivée et confirmée par radio, sans retransfert.
Une lecture indépendante de la partition app1 est identique au fichier
envoyé. Après le redémarrage occasionné par cette lecture, OTA-B répond
toujours via F4MLV-10, sans confirmation en attente et avec le Wi-Fi coupé.

Après la session, F4MLV-10 indique le profil initial et aucune session active.
Ses compteurs sont passés de 7 à 10 241 requêtes retransmises et de 7 à
10 101 réponses retransmises. Il ne déclare aucune erreur d'émission.
Ces compteurs ne constituent pas une mesure exhaustive des pertes RF.

Les preuves sont dans `logs/lora-ota-multihop-20261030/` : journal radio,
synthèse et compteurs du relais. La durée indiquée exclut l'armement,
la négociation initiale, les changements de profil et les essais de
retour arrière et de confirmation.

Ce résultat porte sur un seul relais, une cible et un chemin imposé de
laboratoire. Il ne valide ni les chemins multiples, ni la reprise du
transfert après reset, ni les conditions d'un site distant. Le Wi-Fi
du relais est resté actif pour la gestion ; le transport du firmware
et les commandes de la cible sont passés par la radio.

## Envoi groupé et reprise persistante — essai complet en cours

Une nouvelle version, développée au-dessus de `445c861`, ajoute les groupes
de 32 blocs de 128 octets, un cache de groupe sur le relais, les bilans par
bitmap et un journal de progression par page de 4 096 octets sur la cible.
Les douze tests Python et les quatre bancs C++ (session, profils, enveloppe
de relayage et groupes) passent, ces derniers sous ASan/UBSan. Les trois
firmwares expérimentaux compilent.

Le nouvel essai radio utilise MLV-2 → MLV-10 → MLV-15, avec le même profil
rapide et la cible sans Wi-Fi. Les premières observations établissent :

- deux pages complètes, soit 8 192 octets, enregistrées avant le reset ;
- une demi-page supplémentaire, soit 2 048 octets, présente seulement en RAM ;
- un redémarrage demandé par une commande radio de diagnostic ;
- une nouvelle session authentifiée, avec reprise à 8 192 octets ;
- la poursuite du transfert à partir de cette progression conservée ;
- une réparation locale comptée par le relais après la perte volontaire
  d'un bloc sur sa liaison vers la cible. Les trous restant côté contrôleur
  ont été réparés par retransmission sélective.

L'image de cet essai fait 1 764 720 octets, SHA-256
`ba729503c10820b950faf93262dd957f2e8107ab7ef734489cc2b778a80ca0b2`.
Le journal est conservé dans
`logs/lora-ota-window-20261030/radio-update-validated.jsonl`, avec les
compteurs du relais après les premières pages.

Le transfert complet et la mesure de son gain restent en cours. Ces premières
observations ne valident ni son intégrité finale, ni une coupure électrique
pendant l'écriture de flash ou du journal NVS. Les interruptions à ces deux
endroits ont uniquement été simulées dans le banc C++.
