# Blocs OTA sur une liaison relayée et atténuée : essais du 8 novembre 2026

[English version](OTA_BLOCK_SIZES_RELAY_ATTENUATED.md)

## Objet et limites

Cette campagne compare six tailles de blocs OTA sur une liaison relayée et
dégradée. Chaque passage transmet un échantillon de **65 536 octets**, relu
dans la partition inactive et vérifié par SHA-256. Il ne valide ni une image
complète ni son activation.

SHA-256 de l'échantillon :
`d60ff2652f6fe489ae5d816451a5696d545c802565eb464438132aa4c0617252`.

## Montage

Le trajet est **MLV-2 -> MLV-10 -> MLV-15** :

- MLV-2 est le contrôleur. Il utilise le pont OTA intégré au firmware iGate
  OTA-A, sans firmware de pont séparé ;
- MLV-10 est le relais ;
- MLV-15 est la cible et porte la cascade d'atténuation ;
- le profil de contrôle est 434,100 MHz, BW125, SF12, CR4/5, 20 dBm ;
- le profil de transfert est 434,500 MHz, BW500, SF7, CR4/5 ;
- aucune image reçue n'est activée.

Le pont intégré a tenu le rôle de contrôleur pendant toute la série. Une
correction a été nécessaire avant la campagne : l'ouverture du port série
redémarrait la carte.

## Atténuation et niveau reçu

La cascade est constituée de deux sections utilisant au total deux
résistances de 820 ohms et quatre résistances de 51 ohms. Son insertion
mesurée au NanoVNA est de **53 dB**. Sur la liaison radio, l'écart observé
n'est que de **28 dB** ; ces deux valeurs ne doivent donc pas être
confondues.

Avec MLV-15 réglé à 2 dBm, MLV-10 le reçoit à **-113 dBm** avec un SNR de
**+3,75 dB**. Cette condition est légèrement plus défavorable que les
liaisons SOMTNP <-> WHTANK et PINAL décrites par N7UV.

## Série à 2 dBm

La série à 2 dBm s'arrête dès le premier passage. La liaison est perdue
après l'écriture de **36 864 octets**. Ce passage ne fournit donc aucune
comparaison entre tailles de blocs.

## Série à 9 dBm

À 9 dBm, **21 passages sur 22** aboutissent. Trois répétitions complètes des
six tailles ont été obtenues. Une quatrième répétition partielle ajoute
trois succès, puis s'interrompt sur les blocs de 160 octets.

Les durées ci-dessous sont les médianes des trois répétitions complètes :

| Bloc utile (octets) | Médiane pour 64 Ko (s) |
|---:|---:|
| 128 | 135 |
| 175 | 137 |
| 160 | 144 |
| 96 | 190 |
| 64 | 228 |
| 34 | 370 |

Dans les conditions de cet essai, les blocs de **128 à 175 octets** donnent
les transferts les plus rapides. Les blocs de 34 à 96 octets n'apportent
pas de gain de robustesse compensant leur surcoût. L'écart de deux secondes
entre 128 et 175 octets, sur trois répétitions, ne suffit pas à les départager
définitivement.

Environ la moitié du temps du banc est consacrée à une dizaine d'échanges de
contrôle en SF12 pour chaque passage. Dans une mise à jour complète sans
interruption, cette négociation n'a lieu qu'une fois et son poids relatif est
donc beaucoup plus faible.

## Coupures à 36 864 octets

Les deux coupures retenues, l'une à 2 dBm et l'autre à 9 dBm, ont la même
forme et surviennent après **36 864 octets** écrits. Les bilans de page ne
reçoivent plus de réponse, la liaison est déclarée perdue, puis la tentative
de reprise échoue. Aucun redémarrage d'une carte n'est observé.

### Séquence de reprise observée

Une relecture des traces après la campagne invalide le diagnostic initial d'un
ancien nonce réutilisé. Dans les deux échecs, le banc suit déjà cette séquence :

1. il revient au profil de contrôle et attend 65 secondes, soit le bail de
   60 secondes augmenté d'une marge de cinq secondes ;
2. il envoie un nouveau `HELLO` en SF12 et reçoit une réponse avec un nouveau
   nonce ;
3. il envoie `PREPARE_FAST` en SF12 et en reçoit l'acquittement ;
4. il passe le contrôleur au profil BW500/SF7 ;
5. les huit tentatives de `STATUS` en profil rapide restent sans réponse ;
6. les trois tentatives de `RETURN_NORMAL` dans ce même profil restent aussi
   sans réponse ;
7. le contrôleur revient alors au profil normal, mais les quatre tentatives de
   `STATUS` en SF12 restent sans réponse.

La reprise échoue donc après la réouverture correcte de la session, lors de la
confirmation du retour au profil rapide. Les traces du contrôleur ne permettent
pas de déterminer si la cible ou le relais ne bascule pas correctement, si le
premier `STATUS` ne leur parvient pas, ou si sa réponse est perdue.

Le silence final en SF12 ne démontre pas que la cible est restée en profil
rapide. La première tentative normale commence alors que son bail rapide peut
encore être actif. À l'expiration du bail, le firmware revient au profil normal,
appelle `cancel()` et invalide le nonce ; les tentatives suivantes portant ce
nonce sont alors ignorées. Seule la console de la cible permettra de dater la
bascule et l'expiration de session.

### Cause initiale non établie

Les journaux du contrôleur ne permettent pas d'expliquer la perte radio
initiale :

- avant la coupure à 9 dBm, les bilans de page demandent déjà une seconde
  tentative toutes les deux ou trois pages, ce qui indique une dégradation
  progressive plutôt qu'une coupure brutale ;
- après une renégociation réussie en SF12, aucune trame ne passe plus en
  BW500/SF7, alors que ce profil fonctionnait quelques minutes auparavant ;
- l'offset identique de 36 864 octets dans les deux échecs peut être fortuit.

Il n'est donc pas possible d'attribuer la perte à la taille des blocs, au
profil rapide, au relais ou à la cible avec les seules traces disponibles.

## Suite nécessaire

Avant une nouvelle série complète :

1. reproduire uniquement la perte et la reprise, sans rejouer toute la matrice
   des tailles de blocs ;
2. enregistrer simultanément la console série de MLV-15, qui indique la
   réception des opérations OTA et les transitions de profil ;
3. conserver les traces du contrôleur et du relais afin de distinguer la
   réception de `PREPARE_FAST`, la bascule effective et la réception du premier
   `STATUS` en profil rapide ;
4. ne modifier la logique de reprise qu'après avoir localisé l'étape fautive.

Les traces, résultats CSV/JSON et manifestes de cette campagne sont conservés
localement dans `logs/block-cascade-20261108/` et
`logs/link-calibration-20261108/`.

La [campagne ciblée du 11 novembre](../2026-11-11/OTA_PROFILE_RECOVERY_ATTENUATED_fr.md)
ne reproduit pas l'offset fixe de 36 864 octets. Elle démontre deux reprises
durables complètes et localise les échecs restants sur les réponses du profil
rapide dans ce montage atténué.
