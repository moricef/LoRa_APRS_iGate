# Mise à jour des firmwares LoRa APRS par radio à travers plusieurs relais

**Résultats expérimentaux, architecture et suite du développement**\
**Document préparé pour Jon N7UV — mise à jour du 30 octobre 2026**

## 1. Objectif

Permettre la mise à jour des iGates et digipeaters LoRa APRS sur ESP32 par radio, sans connexion IP ni intervention physique sur la cible distante. À terme, distribuer une image à travers plusieurs relais vers un ou plusieurs équipements, avec reprise après interruption et retour au fonctionnement APRS habituel.

**Une première réalisation de la distribution contrôlée est maintenant validée en laboratoire : un contrôleur, un relais et une cible, avec envoi groupé, réparation locale et reprise après redémarrage de la cible.** Les chemins multiples et la distribution à plusieurs cibles restent à développer.

## 2. Résultats des essais

Les essais ont utilisé des TTGO LoRa32 V2.1 équipées d’ESP32 et de radios SX1278. Le montage relayé est **F4MLV-2 → F4MLV-10 → F4MLV-15**. Le prototype est intégré au firmware iGate qui comprend RXT, le format natif et le décodage PPM autonome.

Le profil rapide commun aux essais est **BW500 kHz, SF7, CR4/5, puissance 2 dBm**, soit **21,875 kbit/s nominaux**. Le profil initial du banc est 434,100 MHz, BW125/SF12/CR4/5 ; le profil rapide utilise 434,500 MHz. Ces paramètres sont propres au laboratoire.

| Essai | Image, en octets | Transfert et vérification | Débit utile |
|---|---:|---:|---:|
| Liaison directe, cible sans Wi-Fi | 1 744 480 | 26 min 38 s | 1 092 octets/s |
| Un relais, acquittement par bloc | 1 760 528 | 1 h 08 min 18 s | 430 octets/s |
| Un relais, groupes et reprise persistante | 1 764 720 | 53 min 24 s | 551 octets/s |

Les durées couvrent le transfert des données et la vérification de l’image, pas la préparation initiale, la négociation initiale du profil, l’activation ni la confirmation du démarrage. Le dernier essai inclut **73,24 secondes de récupération volontaire après redémarrage pendant le transfert**.

Dans le montage relayé, l’envoi groupé a réduit la durée observée d’environ **22 %** et augmenté le débit utile d’environ **28 %**. Les images ont des tailles voisines mais différentes. Cette comparaison porte sur deux essais successifs, pas sur un gain garanti pour tous les réseaux ni sur une mesure directe du temps d’antenne cumulé.

La cible avait son Wi-Fi et son point d’accès coupés. Cet état a été vérifié par des réponses radio authentifiées avant et après la mise à jour. Le Wi-Fi du relais est resté actif pour sa gestion. Les blocs du firmware et les commandes de la cible ont circulé par radio. La préparation initiale et la lecture indépendante de la flash ont utilisé l’USB.

## 3. Ce qui a été vérifié

L’image complète a été écrite dans la partition inactive de l’ESP32, contrôlée par SHA-256, puis validée comme image ESP32. Une lecture indépendante de cette partition par USB est identique octet par octet au fichier envoyé.

L’activation reste une commande authentifiée distincte. Deux retours à l’ancien firmware ont été observés : après un reset provoqué avant confirmation, puis après une activation laissée sans confirmation au-delà du délai. Le nouveau firmware a ensuite été activé et confirmé par radio. Après un redémarrage supplémentaire, il répondait encore par le relais, sans Wi-Fi.

Le passage au profil rapide et le retour au profil initial ont fonctionné. Les contrôles précédant le transfert ont aussi montré que la cible ne répondait pas à une commande directe du contrôleur ni lorsque le relais était suspendu ; elle répondait après réactivation du relais.

La réception APRS texte, native et PPM avait été vérifiée lors des essais directs précédents. Elle n’a pas été remesurée dans le dernier essai groupé.

## 4. Fonctionnement de l’envoi groupé

Le contrôleur envoie un groupe de **32 blocs de 128 octets**, soit une page de flash de **4 096 octets**, sans attendre un acquittement par bloc. La dernière page peut être plus courte.

Le relais écoute ce groupe et conserve les trames signées en RAM. Sur demande du contrôleur, il les transmet à la cible, puis lui demande un bilan de réception sous forme d’un masque de 32 bits. Le contrôleur et le relais émettent ainsi à tour de rôle.

Si des blocs manquent à la cible, le relais les retransmet depuis son cache lorsqu’il les possède. Les trous restants sont signalés au contrôleur, qui ne renvoie que les blocs encore manquants. Le cache conserve **un groupe**, pas une image complète, et disparaît après redémarrage du relais.

Sur le transfert complet, le relais a compté **94 réparations locales de blocs** et le contrôleur **17 retransmissions demandées**. Une perte ponctuelle avait été provoquée sur chaque liaison. Ces compteurs ne constituent pas un taux de pertes RF ni un diagnostic de leur cause.

## 5. Reprise persistante : ce qui est conservé

La cible accepte les blocs dans le désordre à l’intérieur du groupe courant et compare les doublons. Une page complète est écrite dans la partition inactive. Ensuite seulement, un journal NVS conserve l’identité de l’image et l’offset durable atteint. Les blocs d’une page incomplète restent en RAM.

L’essai a enregistré **8 192 octets**, puis reçu **2 048 octets supplémentaires** sans terminer la page suivante. Un reset commandé par radio a redémarré la cible. Après retour aux profils initiaux et ouverture d’une nouvelle session authentifiée, elle a retrouvé les **8 192 octets enregistrés**. Seule la page incomplète a été retransmise ; les pages conservées n’ont pas été renvoyées.

L’expiration d’une session conserve cette progression ; une annulation explicite l’efface. La reprise utilise une nouvelle authentification et ne réutilise pas l’ancien nonce. Le client dispose aussi d’une récupération après perte de liaison, mais le redémarrage physique du relais pendant un transfert reste à éprouver.

**Un reset de la cible a été testé sur carte. Les coupures électriques pendant l’écriture d’une page ou du journal n’ont été que simulées dans le banc C++ : leur résistance sur matériel reste à vérifier.**

## 6. Les deux architectures du projet

### Architecture 1 — Distribution contrôlée

Un contrôleur sélectionne les participants et organise l’envoi des blocs. Les relais peuvent les conserver en cache ; les récepteurs signalent les blocs manquants pour une réparation sélective.

Le prototype en réalise maintenant une première version : **un chemin imposé, un relais, une cible et un cache de groupe**. La distribution partagée vers plusieurs cibles, les caches plus étendus et les chaînes de plusieurs relais successifs ne sont pas encore réalisés.

### Architecture 2 — Plusieurs chemins vers une cible

Deux chemins ou davantage pourraient assurer la livraison lorsqu’une liaison devient indisponible. Cela demande de coordonner les transmissions, d’éliminer les doublons et de gérer séparément les états des chemins.

Cette extension reste à développer. Elle complète la distribution contrôlée ; elle ne la remplace pas. La recommandation reste de prendre l’architecture 1 comme base et d’ajouter les chemins alternatifs lorsque leur utilité est démontrée.

## 7. Commande et changement de profil

La préparation, le changement de profil, l’activation et la confirmation utilisent des commandes binaires OTA authentifiées. Le « profil APRS habituel » désigne ici les paramètres radio enregistrés, pas des commandes transportées comme messages APRS classiques. Le banc utilise temporairement son propre profil initial.

Le contrôleur prépare la session sur ce profil initial. La cible acquitte le changement ; le relais retransmet l’acquittement, puis chacun passe au profil rapide après son délai local. Le contrôleur vérifie le lien avant de transférer les données. Une fois l’image vérifiée, les participants reviennent au profil initial pour l’activation et sa confirmation.

Avec une seule radio, un équipement passé au profil rapide n’écoute plus le profil initial. Les commandes de retour pendant le transfert utilisent donc le profil rapide. Un bail local d’inactivité rétablit les paramètres enregistrés si la liaison disparaît ; le rétablissement ne dépend pas d’une dernière commande reçue. Un redémarrage recharge également le profil enregistré.

Ce mécanisme fonctionne pour le chemin essayé. La coordination de plusieurs relais et de profils différents par liaison reste un développement distinct.

## 8. Authentification, intégrité et démarrage

Le protocole comporte déjà les identités du contrôleur et de la cible, un compteur d’armement persistant, un nonce de session, les offsets et longueurs des blocs, un manifeste authentifié et des HMAC. L’enveloppe du relais ajoute l’identité de l’émetteur du saut et une authentification de transport distincte ; le relais conserve intacte la signature destinée à la cible.

Le manifeste identifie la taille de l’image, son SHA-256 et la carte de laboratoire attendue. SHA-256 contrôle le contenu ; son empreinte attendue est protégée par l’authentification du manifeste. L’image ESP32 est vérifiée avant activation et la sélection du nouveau firmware reste séparée de sa réception.

La confirmation radio du démarrage et le retour à l’ancienne application sont indépendants de la reprise du téléchargement. Une image incomplète ne doit jamais être sélectionnée pour démarrer. L’extension à d’autres cartes et plans de partitions demandera ses propres contrôles de compatibilité.

## 9. Matériel et débits

Le profil BW500/SF7/CR4/5 peut servir de profil commun aux SX1278 et SX1262. Les essais décrits ici n’ont toutefois utilisé que des SX1278.

Le SX1262 accepte SF5 : BW500/SF5/CR4/5 donne **62,5 kbit/s nominaux**. Le SX1278 ne permet pas SF5. Ce débit n’a donc pas été essayé. Des profils différents par liaison pourraient optimiser un réseau mixte, mais ils ne sont pas nécessaires pour commencer avec un profil commun compatible.

Ces débits physiques ne sont pas les débits utiles : les en-têtes, les deux sauts, les changements émission/réception, les bilans et les réparations comptent dans la durée du transfert.

## 10. Première installation sur les sites

La voie OTA examinée dans le firmware de Ricardo utilise ElegantOTA et le serveur Web de l’ESP32. Elle transporte le firmware par IP et ne reçoit pas les blocs de ce protocole par LoRa.

Un équipement déjà installé doit donc recevoir une première fois un firmware compatible, par mise à jour Web s’il est accessible ou par flashage local. Le prototype expérimental décrit ici n’est pas publié comme version de déploiement pour les sites isolés.

## 11. Validations restantes

| Sujet | État |
|---|---|
| Un relais, image complète, cible sans Wi-Fi | Validé en laboratoire |
| Groupes, cache et réparation sélective | Validés sur le transfert complet |
| Reprise après reset de la cible | Validée sur carte |
| Coupures électriques pendant écriture flash/NVS | Simulées sur hôte ; essais matériels à faire |
| Redémarrage ou panne du relais pendant le transfert | À tester |
| Plusieurs relais successifs | À développer et tester |
| Chemins alternatifs et plusieurs cibles | À développer et tester |
| SX1262/SF5 et profils différents par liaison | Non mesurés |
| Sites éloignés, stations cachées, asymétrie, canal chargé | Essais de terrain à faire |

La résistance aux coupures, la gestion des chemins et la coexistence avec le trafic APRS doivent être éprouvées avant de considérer ce mécanisme comme prêt pour des sites inaccessibles.

## 12. Bilan

**Le premier fonctionnement de l’architecture 1 est démontré sur un chemin à deux sauts.** L’envoi groupé, la réparation locale et la reprise persistante fonctionnent ensemble sur une image complète, avec intégrité vérifiée, activation séparée, confirmation radio et retours arrière.

La suite porte sur les pannes réelles pendant l’écriture et sur le relais, puis sur l’extension à plusieurs relais, chemins et cibles. L’architecture générale du projet n’est pas encore entièrement validée.

Le [code expérimental](https://github.com/moricef/LoRa_APRS_iGate/tree/experiment/lora-ota) et le [compte rendu du 30 octobre](https://github.com/moricef/LoRa_APRS_iGate/blob/experiment/lora-ota/docs/tests/2026-10-30/OTA_LORA_fr.md) sont accessibles sur la branche `experiment/lora-ota`. Le code testé correspond à `8241529`, le compte rendu à `043af4d`. Il s’agit d’une publication de sources et de résultats expérimentaux, pas d’une version de firmware proposée pour le déploiement.
