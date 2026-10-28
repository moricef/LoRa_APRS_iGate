# Un format de trame natif pour LoRa APRS

LoRa APRS transporte aujourd'hui les paquets APRS sous forme de texte.
Cette représentation permet l'interconnexion avec APRS-IS, mais conserve
des contraintes héritées de l'adressage AX.25 et de la syntaxe APRS.
Un format natif LoRa peut utiliser des champs explicites et des codages
compacts tout en conservant la traduction vers l'APRS standard.

Ce document décrit l'architecture visée. Le format du brouillon 0,
déjà implémenté, est décrit dans
[LORA_APRS_NATIVE_FRAME_DRAFT0_fr.md](LORA_APRS_NATIVE_FRAME_DRAFT0_fr.md).
Il ne définit pas encore une structure binaire distincte pour chacun
des types de paquet énumérés ci-dessous.

## Limites de la représentation actuelle

APRS mêle données structurées et commentaire. Les marqueurs /A= et PHG,
ainsi que les règles de codage du cap et de la vitesse, nécessitent une
interprétation du champ d'information. Mic-E utilise également le champ
destination pour coder des données. Les implémentations peuvent interpréter
les mêmes octets différemment.

RXT v1 a introduit une ambiguïté supplémentaire en ajoutant les mesures
des récepteurs à la fin du commentaire. Un commentaire utilisateur qui se
termine par le même motif peut être pris pour du RXT.

Le texte occupe aussi du temps d'antenne. La source, la destination, le
chemin et le champ d'information sont transmis dans chaque paquet.
Les commentaires et chemins longs augmentent l'occupation du canal,
particulièrement en SF12.

## Codage radio et version de trame

Le format proposé sépare les adresses, l'état du relayage, les données
applicatives, le texte libre et les mesures RXT. Position, cap, vitesse,
altitude, météo, télémétrie et messages doivent avoir des représentations
définies. Le texte libre doit être explicitement délimité ; ses marqueurs
ne doivent plus servir à en déduire des champs supplémentaires.

L'adressage natif doit accepter des indicatifs de longueur étendue et des
SSID alphanumériques tels que -MC et -GS, sans reprendre les limites du
champ d'adresse AX.25. La spécification doit fixer les longueurs maximales.
Les passerelles doivent traiter explicitement les limites propres à
APRS-IS et à AX.25 : une identité acceptée sur LoRa n'est pas nécessairement
représentable sur ces interfaces.

Le préfixe texte actuel est 3C FF 01. Le brouillon 0 utilise 3C FF 02.
Les récepteurs doivent sélectionner le décodeur selon le type de trame et
rejeter les types inconnus avant toute analyse textuelle. Un équipement
sans décodeur natif ne peut ni recevoir ni relayer le trafic natif.

## Traduction vers et depuis APRS-IS

L'iGate traduit les paquets natifs pris en charge en APRS texte standard
avant leur transmission à APRS-IS. Pour les messages et objets descendants,
il traduit l'APRS texte dans le format radio sélectionné pour l'émission.

APRS-IS et les applications APRS existantes continuent de recevoir des
paquets standard. Les identités ou champs natifs non représentables
nécessitent une règle explicite de passerelle ; chaque implémentation
ne doit pas inventer sa propre correspondance ou les tronquer silencieusement.

## Traduction déterministe

Pour un même paquet natif, des décodeurs indépendants doivent restituer
des octets identiques pour la source, la destination APRS et le champ
d'information. APRS-IS compare ces champs pour détecter les doublons ;
le chemin de transport extérieur peut différer selon l'iGate.

RXT est une métadonnée radio et doit rester séparé du champ d'information
envoyé à APRS-IS. Les anciens trailers visibles illustrent le problème :
si un iGate conserve le trailer et qu'un autre le retire, les champs
d'information deviennent différents.

La spécification doit fixer les règles exactes de traduction et fournir
des vecteurs de référence pour l'encodage, le décodage et la sortie
des passerelles.

## Données applicatives et état du relayage

Le périmètre visé comprend :

- position, cap, vitesse et altitude ;
- statut ;
- messages, ACK et reply-ACK ;
- télémétrie ;
- météo ;
- objets et items ;
- requêtes ;
- trames tierces ;
- chemins de relais et mesures RXT.

L'état du relayage doit distinguer explicitement les relais demandés,
les sauts consommés et les sauts restants. Les relais doivent pouvoir
mettre cet état à jour sans le reconstruire à partir des réécritures
textuelles de WIDEn-N.

## Codec commun et tests de conformité

Une bibliothèque C++ embarquable peut servir au Tracker et à l'iGate.
Les implémentations indépendantes, notamment en Go si nécessaire, doivent
passer les mêmes vecteurs de référence.

Les tests doivent couvrir la restitution exacte, les entrées mal formées,
les versions inconnues, les limites d'adressage, les modifications du chemin
et le rattachement des mesures RXT à leurs relais. Un corpus de trames
capturées complète les cas de compatibilité.

## Intégration dans l'iGate et le digipeater

Le firmware de l'iGate et du digipeater doit :

- recevoir le texte historique et les types natifs pris en charge ;
- mettre à jour l'état du relayage et conserver l'association de chaque mesure RXT ;
- dédupliquer sur le contenu APRS décodé ;
- traduire vers et depuis APRS-IS en respectant RFONLY et NOGATE ;
- conserver la messagerie, les ACK, reply-ACK et le routage retour appris ;
- conserver les requêtes locales et les commandes distantes authentifiées ;
- gérer les balises et la télémétrie produites localement ;
- fournir des sorties compatibles TNC2, KISS, MQTT, syslog, JSON, journal SD et WebUI.

Un client KISS qui attend de l'AX.25 doit recevoir des trames AX.25 ;
un client TNC2 attend du texte. Ces interfaces ne permettent pas de
représenter toutes les identités natives envisagées. Leurs limites doivent
être documentées et appliquées aux conversions.

Le réglage d'émission doit distinguer les modes historique, natif et double
émission. Émettre dans les deux formats augmente l'occupation du canal :
c'est un mode de transition ou de test, pas un mode d'économie d'antenne.

## Intégration dans le Tracker

Le Tracker doit recevoir les deux formats et permettre les balises et
messages natifs en conservant les conversations, ACK et reply-ACK.

Les interfaces vers des applications telles qu'APRSdroid doivent fournir
la représentation qu'elles attendent. Si le digipeater du Tracker est
activé, il doit aussi mettre à jour l'état du relayage et conserver les
associations RXT.

## Intégration dans Graywolf

Graywolf est un logiciel APRS avec une interface web de cartographie et de
messagerie. Nous l'utilisons aussi pour afficher les mesures des récepteurs
et des relais transmises par l'iGate.

Lorsque l'iGate fournit du TNC2 ou du JSON déjà traduit, Graywolf n'a pas
besoin du codec radio. Une implémentation accédant directement à la radio
LoRa aurait besoin de l'encodage et du décodage natifs.

La spécification de l'interface doit distinguer le paquet APRS, les mesures
du récepteur local et les mesures rapportées par les relais distants.

## Compatibilité et déploiement

Les formats historique et natif doivent coexister pendant le déploiement.
Une mise à jour doit conserver la réception du trafic historique ;
l'émission native nécessite des récepteurs et relais compatibles sur le
trajet radio.

L'adoption dépend aussi du firmware upstream et d'implémentations extérieures
à ce fork. Les essais doivent couvrir les réseaux mixtes et vérifier que les
passerelles produisent une sortie APRS standard cohérente.

## Validation sur réseau pilote

Le réseau local et le réseau RXT de Jon N7UV en Arizona constituent des
terrains d'essai possibles. La prise en charge du natif et son déploiement
doivent être vérifiés pour chaque station participante ; la présence de RXT
ne suffit pas à les établir.

Les mesures du pilote doivent porter sur :

- la taille des trames et le temps d'antenne selon le profil radio ;
- la livraison des paquets, le relayage et la messagerie ;
- l'attribution RXT, y compris les mesures manquantes et les collisions
  d'empreintes lors d'une conversion en texte ;
- la conservation des octets du commentaire ;
- l'identité des traductions produites par des récepteurs indépendants ;
- la coexistence avec les équipements historiques ;
- le temps de traitement et la mémoire sur les cartes cibles.

La spécification, les vecteurs de test, les outils de capture et les
instructions du firmware doivent identifier la révision du format testée.
