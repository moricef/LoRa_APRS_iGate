# RXT à la frontière RF/APRS-IS

## Objectif

Cette note décrit la manière dont la télémétrie RXT doit être traitée lorsqu'un
paquet APRS transite à la fois par le réseau RF LoRa et par APRS-IS. Elle
explique également pourquoi un suffixe RXT laissé passer par un seul iGate peut
empêcher la suppression normale des doublons par APRS-IS et conduire APRS.fi à
signaler une télémétrie dupliquée ou reçue dans le désordre.

RXT est une extension limitée au domaine RF. Elle est utile tant qu'un paquet
transite par RF, mais elle ne doit jamais être incluse dans le paquet envoyé à
APRS-IS.

## Exemple observé

F1ZDB-10 a envoyé le paquet de télémétrie suivant directement à APRS-IS :

```text
2026-09-14 17:41:35 CEST
F1ZDB-10>APLRG1,TCPIP*,qAC,T2ROMANIA:=L8i]3NrQ>a !GLoRa APRS|"/%b!P!/!B|
```

Dix-sept secondes plus tard, le même paquet est arrivé par le réseau RF LoRa,
après avoir été relayé par F4JQT-4 et F4MLV-10, puis envoyé à APRS-IS par
F4INI-10 :

```text
2026-09-14 17:41:52 CEST
F1ZDB-10>APLRG1,F4JQT-4*,F4MLV-10*,qAR,F4INI-10:=L8i]3NrQ>a !GLoRa APRS|"/%b!P!/!B|{,N5H}
```

La source, la destination, la position, le commentaire et le numéro de
séquence de télémétrie Base91 sont identiques. Le seul ajout dans le champ
d'information est le tuple RXT de quatre caractères :

```text
{,N5H}
```

APRS.fi a donc signalé le second paquet comme suit :

```text
Duplicate telemetry sequence
Delayed or out-of-order packet (sequence number)
```

## Pourquoi le doublon atteint APRS.fi

Le paquet Internet direct et sa copie RF représentent le même événement APRS.
Sans le suffixe RXT, APRS-IS peut reconnaître la copie tardive comme un doublon
malgré la différence entre les chemins de transport.

En revanche, si un iGate transmet le suffixe RXT, les champs d'information APRS
ne sont plus identiques :

```text
Direct :   ...|"/%b!P!/!B|
Copie RF : ...|"/%b!P!/!B|{,N5H}
```

La modification du champ d'information empêche le mécanisme normal de
suppression des doublons de considérer qu'il s'agit du même paquet. APRS.fi
reçoit donc les deux paquets et détecte la répétition de leur numéro de séquence
de télémétrie intégré.

Le tuple RXT ne crée pas une nouvelle mesure APRS. Il décrit uniquement la
réception et le relais RF. En dehors du réseau RF, il ne doit pas donner
l'impression que la copie relayée constitue un nouveau paquet APRS.

## Cycle de vie correct du paquet

Un système compatible RXT doit traiter un paquet dans l'ordre suivant :

1. Recevoir le paquet RF complet, y compris tout suffixe RXT existant.
2. Extraire et décoder les tuples existants selon les besoins pour l'affichage
   local, la sortie TNC ou la journalisation.
3. Conserver le suffixe sur le paquet RF destiné à être digipété.
4. Lors d'un véritable relais RF, ajouter le tuple local de quatre caractères.
5. Conserver la chaîne complète de tuples pour les sauts RF suivants.
6. Si le paquet est envoyé à APRS-IS, supprimer la totalité du suffixe RXT
   immédiatement avant son écriture sur la connexion APRS-IS.

Le tuple ne doit pas être supprimé dès la réception du paquet. Cela empêcherait
le digipeater compatible RXT suivant de prolonger la chaîne de tuples
multi-sauts.

De même, le tuple ne doit pas être conservé jusqu'après l'envoi à APRS-IS. La
passerelle RF vers Internet constitue la frontière à laquelle RXT cesse de
faire partie de la trame transmise.

La transformation attendue à cette frontière est la suivante :

```text
Entrée RF :
F1ZDB-10>APLRG1,F4JQT-4*,F4MLV-10*:=L8i]3NrQ>a !GLoRa APRS|"/%b!P!/!B|{,N5H}

Sortie APRS-IS :
F1ZDB-10>APLRG1,F4JQT-4*,F4MLV-10*,qAR,IGATE:=L8i]3NrQ>a !GLoRa APRS|"/%b!P!/!B|
```

## Implémentation dans le firmware

Le firmware corrigé supprime RXT dans
[`APRS_IS_Utils::buildPacketToUpload()`](../src/aprs_is_utils.cpp), après la
construction du chemin APRS-IS et juste avant que le paquet retourné soit
transmis à `upload()` :

```cpp
String buildPacketToUpload(const String& packet) {
    int colonIndex = packet.indexOf(":");
    String packetToUpload = packet.substring(0, colonIndex);

    // Add qAR/qAO and the iGate callsign here.

    packetToUpload += checkForStartingBytes(packet.substring(colonIndex));

    // RXT is an RF-only extension. Remove it at the APRS-IS boundary.
    return LoRa_Utils::stripRxtTrailer(packetToUpload);
}
```

Cet emplacement est intentionnel. Le paquet original reste disponible pour le
chemin du digipeater, tandis que la copie destinée à APRS-IS est normalisée
avant sa transmission.

Le firmware supprime également RXT avant de présenter un paquet aux autres
consommateurs non RF lorsque cela est approprié. Les valeurs brutes et décodées
peuvent néanmoins être conservées dans des champs dédiés du journal local sur
carte SD ; elles ne doivent pas être intégrées au paquet APRS envoyé à
APRS-IS.

## Exigence de déploiement

Tous les iGate capables de recevoir et de transmettre des paquets provenant du
réseau RF RXT doivent appliquer cette règle de frontière. Un seul iGate non mis
à jour suffit pour laisser passer le tuple et reproduire le symptôme du
doublon.

L'exemple se terminant par :

```text
qAR,F4INI-10:...{,N5H}
```

montre que F4INI-10 a envoyé le paquet en conservant son suffixe RXT. F4INI-10
doit donc être mis à jour, ou son chemin d'envoi RF vers APRS-IS doit être
modifié afin d'effectuer la même opération de suppression de RXT.

Les passerelles ne sont pas obligées d'utiliser un firmware identique. Elles
doivent toutefois respecter la même règle d'interopérabilité :

> Conserver RXT lors des sauts RF ; supprimer RXT à chaque frontière RF vers
> APRS-IS.

La seule mise à jour des digipeaters RXT ne suffit pas. Tous les iGate situés
dans la zone de couverture RF et susceptibles de recevoir un paquet RXT doivent
également être compatibles.

## Solutions alternatives et compromis

### `RFONLY`

La station d'origine peut ajouter `RFONLY` à son chemin RF, par exemple :

```text
WIDE2-1,RFONLY
```

Le paquet peut toujours être relayé par RF, mais les iGate compatibles ne
l'enverront pas à APRS-IS. Cela évite le doublon, mais supprime également le
réseau RF comme solution de secours vers APRS-IS lorsque la connexion Internet
directe de la station d'origine tombe en panne.

`RFONLY` constitue donc une politique réseau facultative, et non un substitut
au traitement correct de RXT à la frontière.

Dans ce firmware, `NOGATE` ne doit pas être utilisé comme solution équivalente,
car il est rejeté par le code du digipeater et interrompt également le relais
RF.

### Cache de doublons inter-réseaux

Un iGate pourrait conserver un cache de courte durée des paquets récemment
reçus depuis APRS-IS et empêcher un envoi RF si la même source et le même champ
d'information sans RXT ont déjà été observés.

Cette solution est plus complexe et suppose que l'iGate reçoive à temps la
copie directe provenant d'APRS-IS. Elle peut constituer une protection
supplémentaire, mais n'est pas nécessaire pour corriger la fuite RXT elle-même.

### Fonctionnement en digipeater pur

Transformer la station d'origine ou le relais en digipeater pur supprimerait
également l'un des chemins Internet, mais sacrifierait inutilement des
fonctionnalités. La suppression correcte du suffixe à la frontière de l'iGate
permet un fonctionnement simultané par Internet et par RF sans publier RXT
comme partie de la charge utile APRS.

## Relation avec le défaut `undefined*`

Le défaut de chemin `undefined*` et le problème de doublon APRS-IS sont
distincts, bien qu'ils aient tous deux été révélés par le trafic RXT.

Les anciennes images de configuration pouvaient ne pas contenir
`tacticalCallsign`. L'interface Web pouvait alors enregistrer la valeur
JavaScript `undefined` comme texte de configuration littéral, ce qui amenait un
digipeater à remplacer un alias WIDE par `undefined*`.

Le commit `a027a88` (`Prevent undefined digipeater identities`), créé le
14 septembre 2026 à 14:16:55 CEST, a ajouté trois protections :

- les indicatifs absents dans l'interface Web sont remplacés par des chaînes
  vides ;
- les valeurs sentinelles persistantes `undefined` et `null` sont corrigées
  lors du chargement et de l'enregistrement de la configuration ;
- le digipeating RF est refusé si l'identité de station sélectionnée est vide
  ou contient l'une de ces valeurs sentinelles.

Le firmware corrigé a été publié vers 14:17 et flashé sur F4MLV-10 et F4MLV-2
vers 14:19. F6DEV-10 utilisait encore l'ancien firmware, mais était arrêté
depuis environ 12:30. La dernière apparition de `undefined*` communiquée a eu
lieu à 12:47:15, et aucune récidive n'a été observée dans le trafic fourni
ultérieurement.

Les paquets APRS-IS disponibles ne permettent pas de déterminer quel relais a
produit individuellement `undefined*`, car la valeur incorrecte a précisément
remplacé l'identité nécessaire à cette attribution. Il n'est pas indispensable
d'identifier individuellement le relais pour valider le correctif du firmware :
tous les digipeaters concernés doivent être mis à jour avant leur mise ou
remise en service.

## Procédure de vérification

Après la mise à jour de tous les iGate concernés :

1. Envoyer une balise depuis une station qui publie directement sur APRS-IS et
   transmet simultanément le même paquet par RF.
2. Vérifier localement que la copie RF contient sa chaîne de tuples RXT.
3. Vérifier que chaque digipeater RXT conserve les tuples existants et ajoute
   son propre tuple uniquement lors d'un véritable relais RF.
4. Examiner les diagnostics série ou SD de l'iGate final et vérifier que les
   données RXT ont été reçues et décodées.
5. Examiner le paquet réellement envoyé par l'iGate et vérifier qu'il se
   termine avec la charge utile APRS originale, sans suffixe RXT `{....}`.
6. Vérifier qu'APRS.fi n'affiche plus un second paquet de télémétrie portant la
   même séquence uniquement parce qu'un suffixe RXT a modifié son champ
   d'information.
7. Répéter le test avec chaque iGate capable de recevoir le réseau RXT. Une
   seule passerelle non testée ou non mise à jour peut toujours reproduire la
   fuite.

L'invariant à respecter est le suivant :

```text
Chemin de relais RF : RXT conservé et étendu
Envoi APRS-IS : RXT supprimé
```
