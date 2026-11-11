# Reprise OTA sur une liaison relayée et atténuée : essais du 11 novembre 2026

## Objet

Cette campagne reprend uniquement le cas qui avait interrompu deux passages le
8 novembre à 36 864 octets. Elle vérifie la reprise du profil radio et du point
durable avec la console série de la cible enregistrée pendant toute la série.

Chaque passage transfère un échantillon de **65 536 octets** dans la partition
inactive, puis le fait relire et vérifier par SHA-256. Aucune image n'est
activée.

## Montage

Le trajet est **MLV-2 -> MLV-10 -> MLV-15** :

- MLV-2 est le contrôleur et utilise le pont intégré au firmware iGate ;
- MLV-10 est le relais ;
- MLV-15 est la cible et conserve la cascade d'atténuation du 8 novembre ;
- les blocs utiles font 160 octets ;
- le profil de contrôle est 434,100 MHz, BW125, SF12, CR4/5 ; MLV-2 émet à
  20 dBm, tandis que MLV-10 et MLV-15 utilisent leur profil de base à 2 dBm ;
- le profil de transfert est 434,500 MHz, BW500, SF7, CR4/5, 9 dBm ;
- le bail du profil temporaire est de 60 secondes.

Avant la série, deux défauts du banc ont été corrigés. Le port série du pont
reste ouvert après son passage à 921 600 bit/s afin d'éviter le reset des
cartes CH343. Après la perte de l'acquittement de `RETURN_NORMAL`, le
contrôleur attend désormais l'expiration du bail, ouvre une nouvelle session
par `HELLO`, puis vérifie l'état sur le profil normal. Cette seconde correction
a d'abord été reproduite sur un transfert complet, puis couverte par un test
unitaire.

## Résultats

Cinq passages ont été effectués dans les mêmes conditions. Le banc s'arrêtant
au premier échec, ils sont répartis dans quatre répertoires de résultats.

| Passage | Résultat | Durée | Pertes de lien observées |
|---:|---|---:|---|
| 1 | 65 536 octets vérifiés | 251,6 s | reprise à 8 192 octets |
| 2 | échec avant la première page | - | reprise à 0, puis absence des réponses `STATUS` rapides |
| 3 | échec avant la première page | - | absence des réponses `STATUS` lors de l'entrée en profil rapide |
| 4 | échec après progression | - | reprises à 40 960 puis 45 056 octets ; seconde entrée rapide en échec |
| 5 | 65 536 octets vérifiés | 362,9 s | reprise à 8 192 octets |

Les deux passages complets ont chacun demandé cinq retransmissions du
contrôleur. Le relais n'a effectué aucune réparation locale au passage 1 et
deux au passage 5. Leur temps d'occupation radio cumulé mesuré est
respectivement de 118,48 et 122,57 secondes.

La reprise durable est donc effectivement démontrée deux fois : après une
perte à 8 192 octets, une nouvelle session retrouve les pages enregistrées et
le transfert atteint 65 536 octets avec une vérification SHA-256 réussie.

L'offset fixe de 36 864 octets observé le 8 novembre n'est pas reproduit. Les
pertes de cette série surviennent à 0, 8 192, 40 960 et 45 056 octets.

## Observation de la cible

La console de MLV-15 ne montre qu'un démarrage avant la campagne et aucun
reset pendant les cinq passages. Elle montre aussi la réception et le
traitement de plusieurs opérations `STATUS` dont la réponse n'apparaît pas
dans le journal du contrôleur. Dans ce montage, le trajet retour en
BW500/SF7 est donc le facteur limitant observé. Les traces ne permettent pas
d'attribuer la perte au relais, à la propagation ou au récepteur du
contrôleur.

Le correctif de retour au profil normal fonctionne dans les trois passages en
échec : après l'expiration du bail, un nouveau `HELLO` et un `STATUS` en
SF12 répondent, puis le banc peut fermer proprement la session. Il ne répare
pas la perte des réponses en profil rapide, qui est un problème distinct.

## Conclusion et limites

Deux passages sur cinq aboutissent. Cette condition atténuée est donc trop
instable pour une mise à jour automatique sans surveillance, même si la
reprise durable et le retour au profil normal fonctionnent.

L'essai porte sur un échantillon de 64 Ko, une seule taille de bloc, un seul
trajet et une cascade de laboratoire. Il ne mesure ni le taux de succès d'une
image complète ni celui d'une liaison de terrain. Les journaux restent dans
`logs/block-cascade-20261111-targeted/` et ne sont pas publiés.
