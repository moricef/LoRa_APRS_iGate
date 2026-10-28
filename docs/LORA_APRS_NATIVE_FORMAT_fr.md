# Un format de trame natif pour LoRa APRS

LoRa APRS transporte aujourd'hui les paquets APRS sous forme de texte. Ce format assure la compatibilité avec APRS-IS, mais conserve des contraintes héritées d'AX.25 et de la syntaxe APRS. Certaines ne sont plus nécessaires sur une liaison LoRa native.

## Limites du texte APRS sur LoRa

Le premier problème est l'ambiguïté.

Un commentaire peut contenir `/A=`, du PHG, du cap/vitesse ou d'autres informations que tous les décodeurs n'interprètent pas forcément de la même manière. Mic-E stocke même des données dans le champ destination. RXT v1 a ajouté à son tour un trailer particulier qui peut être confondu avec du texte utilisateur.

Le deuxième problème est le temps d'antenne.

Les positions sont envoyées en ASCII, les chemins utilisent les indicatifs complets et, en SF12, chaque octet supplémentaire coûte du temps sur un canal partagé. LoRa permet de faire plus propre et plus compact sans avoir besoin de modifier APRS-IS.

## Natif sur la RF, APRS standard aux frontières

Conserver l'APRS standard aux frontières, mais utiliser une trame native sur la partie RF LoRa.

Les trames LoRa APRS actuelles utilisent :

```text
<\xFF\x01
```

Un format natif pourrait utiliser :

```text
<\xFF\x02
```

Les nouveaux firmwares accepteraient les deux. Les anciens ignoreraient simplement le nouveau format.

La trame native contiendrait des champs explicites au lieu de déduire leur sens du contexte. Position, vitesse, altitude, météo, télémétrie, messages, informations de relais et RXT auraient chacun une représentation définie.

L'adressage natif permettrait des indicatifs de longueur étendue et des SSID alphanumériques (`-MC`, `-GS`), sans reprendre les limites du champ d'adresse AX.25. Les longueurs maximales seraient définies par le format natif. Les limites propres à APRS-IS et aux interfaces AX.25 resteraient à traiter explicitement aux passerelles ; une identité acceptée sur la liaison LoRa ne serait pas nécessairement représentable sur ces interfaces.

Le commentaire resterait du texte libre. Il ne serait plus parcouru pour y rechercher des champs cachés.

## Traduction au niveau de l'iGate

L'iGate devient la frontière de compatibilité.

Côté RF :

```text
trame LoRa native
```

Vers APRS-IS :

```text
APRS texte standard
```

Dans l'autre sens, par exemple pour un message ou un objet :

```text
APRS-IS
   ↓
traduction iGate
   ↓
trame LoRa native
```

Pour APRS-IS, aprs.fi ou un utilisateur VHF, rien ne doit changer.

Le but n'est donc pas de remplacer APRS-IS.

## Traduction déterministe

La traduction vers l'APRS texte doit être déterministe.

Si deux iGates reçoivent exactement la même trame RF native, ils doivent produire exactement le même paquet APRS, octet pour octet.

Sinon APRS-IS peut voir deux paquets différents au lieu d'un doublon.

On a déjà un petit exemple de ce type de problème avec RXT v1 : une information uniquement RF peut modifier ce qu'un iGate retire ou conserve avant l'upload.

Les règles de traduction ne peuvent donc pas dépendre de l'implémentation. Il faut des vecteurs de test avec une sortie attendue exacte.

## Périmètre minimal utile

Il n'est pas nécessaire de traiter immédiatement tous les cas exotiques d'APRS.

Il faut par contre assez de fonctions pour faire tourner un vrai réseau :

- position, cap, vitesse et altitude ;
- statut ;
- messages, ACK et reply-ACK ;
- télémétrie ;
- météo ;
- objets et items ;
- requêtes ;
- trames tierces ;
- chemins de relais ;
- un mécanisme propre d'extension pour RXT.

L'état du relais devrait lui aussi être représenté directement dans la trame. Il faut savoir quels relais étaient demandés, lesquels ont déjà été utilisés et ce qu'il reste à faire, sans reconstruire tout ça à partir des réécritures textuelles de `WIDEn-N`.

## Codec commun et vecteurs de référence

Le tracker et l'iGate devraient utiliser les mêmes règles de codec.

Une petite bibliothèque C++ embarquable avec des tests hôte suffit pour les deux.

Graywolf peut soit porter les mêmes règles en Go, soit vérifier son implémentation à partir des mêmes vecteurs de test.

Le point important n'est pas le langage. C'est d'éviter trois implémentations qui finissent par interpréter le format différemment.

## Modifications iGate et digipeater

Supporter le nouveau format ne consiste pas seulement à le décoder.

L'iGate/digi doit aussi :

- recevoir les anciennes et les nouvelles trames ;
- relayer correctement le chemin natif ;
- transporter RXT sans le cacher dans le commentaire ;
- dédupliquer sur le paquet décodé ;
- traduire vers et depuis APRS-IS ;
- conserver les requêtes, les ACK et les commandes distantes authentifiées ;
- continuer à alimenter correctement TNC2/KISS, MQTT, syslog, JSON, le journal SD et la WebUI.

Pendant la transition, il faudra probablement un choix d'émission du genre :

```text
historique
natif
les deux
```

## Modifications tracker

Le tracker a le même problème de compatibilité.

Il doit recevoir les deux formats, émettre ses balises et ses messages en natif, garder ACK et reply-ACK fonctionnels, et continuer à fournir de l'APRS texte normal aux applications externes comme APRSdroid lorsque c'est ce que l'interface attend.

Si le digipeater intégré est activé, il doit également comprendre directement l'état de relais natif.

## Graywolf

Graywolf est plus simple s'il reçoit déjà du TNC2 ou du JSON traduit par l'iGate.

Dans ce cas, il n'a pas besoin de connaître le format RF.

Il n'a besoin du codec natif que s'il émet directement vers la LoRa.

## Coexistence avec le format actuel

La vraie difficulté n'est pas d'écrire le format. C'est de le déployer sans casser le réseau existant.

Le firmware CA2RXU et les autres implémentations LoRa APRS ne sont pas sous notre contrôle. Il faudra donc faire cohabiter l'ancien et le nouveau format pendant un certain temps.

Un réseau mixte doit malgré tout continuer à produire de l'APRS normal sur APRS-IS.

C'est le test de compatibilité le plus important.

## Déploiement pilote

Un réseau pilote apportera plus qu'une longue spécification théorique.

Le réseau de Jon en Arizona utilise déjà ce fork et RXT, et notre propre réseau fournit un deuxième terrain d'essai.

Les mesures utiles sont assez concrètes :

- temps d'antenne gagné ;
- attribution RXT sans liste blanche ;
- commentaires jamais abîmés ou tronqués ;
- traduction APRS identique entre plusieurs iGates recevant la même trame ;
- comportement d'un réseau mélangeant ancien et nouveau format.

RXT v2 est un bon premier essai parce qu'il ne modifie qu'une petite partie du système.

Si cette transition fonctionne proprement sur les deux réseaux, alors un format de trame natif complet devient une suite beaucoup moins spéculative.
