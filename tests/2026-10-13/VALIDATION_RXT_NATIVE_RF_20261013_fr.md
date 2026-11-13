# Conservation de l'identité RXT native — essais RF du 13 octobre 2026

> Compte rendu historique. Les conclusions portent sur les versions et les conditions décrites dans ce document. Pour les références actuelles, consulter l’[index documentaire](../../README.md).

Le correctif conserve l'association entre une mesure native et son élément
de chemin pendant la réception, le relais et l'attente en file. L'objectif
est de vérifier cette association dans la trame effectivement retransmise,
en présence de deux indicatifs partageant une empreinte.

## Montage et méthode

F4MLV-2 et F4MLV-15 sont deux TTGO LoRa32 v2.1 au même emplacement.
Profil de test : 433,900 MHz, SF7, BW 125 kHz, CR 4/5, puissance 2 dBm,
CAD désactivé. F4MLV-15 fonctionne en relais de chemin explicite, émission
native. F4MLV-10 ne participe pas au montage.

F4MLV-2 émet des trames natives construites avec le codec. Leur chemin
contient F4MLV-18 et F6DEV-10, dont l'empreinte est `C`, avec une seule
mesure placée sur F6DEV-10. Ces deux noms sont des éléments du chemin de
test, pas des équipements participant physiquement à l'essai. Les valeurs
`bbbb` et `cccc` sont injectées pour vérifier leur conservation.

F4MLV-15 reçoit et relaie. L'identité affichée est contrôlée dans ses
événements JSON. Les octets de sa retransmission sont capturés par le modem
de F4MLV-2 et décodés sur l'hôte ; une mesure affichée correctement ne
suffit donc pas à valider le relais.

Le récepteur utilise le binaire corrigé sans instrumentation pour les quatre
premiers cas : build `2026-10-13 15:56:12 UTC`, SHA-256
`66bff0b00daca439e756b307a876bdb4bdd3531f2ffd850a351ae3b4ebd7e2d5`.
L'émetteur utilise une copie instrumentée pour l'injection et la capture
des octets radio. Pour le cas différé, le récepteur utilise cette même copie,
qui retient uniquement les paquets marqués `NRF-DEFER` sept secondes sans
bloquer les réceptions. Les fonctions d'attribution et de réencodage sont
celles du correctif ; l'instrumentation ne modifie ni les mesures ni leurs
propriétaires.

## Observations concluantes

| Cas | Observation |
| --- | --- |
| `NRF-ALIAS-161713` | La mesure reçue à l'index 2 reste attribuée à F6DEV-10. Après retrait de `WIDE1*`, elle est retransmise à l'index 1, toujours `bbbb`. La mesure locale de F4MLV-15 est à l'index 2. |
| `NRF-PLAIN-161824` | Sans alias à retirer, `bbbb` reste à l'index 1 sur F6DEV-10, avec la nouvelle mesure locale à l'index 2. |
| `NRF-COMMENT-161713` | Le commentaire terminé par `{Cbbbb}`, qui ressemble à un trailer texte v2, reste inchangé dans le relais natif. La mesure séparée reste sur F6DEV-10. |
| `NRF-IS-161927` | La copie observée sur APRS-IS conserve exactement `>NRF-IS-161927 {Cbbbb}`. Aucun RXT n'est ajouté au champ d'information. Le relais RF natif conserve également `bbbb` et son propriétaire. |
| `NRF-DEFER-162139` puis `NRF-AFTER-162139` | Le second paquet est reçu avant la retransmission du premier. Les retransmissions contiennent respectivement `bbbb` et `cccc` sur F6DEV-10 ; la nouvelle réception n'a pas remplacé les mesures du paquet en attente. |

Pour le cas différé, les horodatages de la capture donnent :

- première réception JSON : `16:21:55.648397 UTC` ;
- seconde réception JSON : `16:21:57.657004 UTC` ;
- première retransmission capturée en RF : `16:22:02.684851 UTC` ;
- seconde retransmission capturée en RF : `16:22:02.884774 UTC`.

## Traces et portée

Les résultats et captures sont conservés dans
`logs/native-rf-20261013/` : `basic-results.json`, `plain-results.json`,
`boundary-results.json`, `deferred-results.json` et les fichiers
`*-capture.jsonl`. Le décodeur hôte, le pilote d'essai et les sources
instrumentées sont dans ce même dossier, ignoré par git.

Les observations valident le maintien de l'identité reçue, le déplacement
d'index après suppression d'un alias, le commentaire opaque, la frontière
APRS-IS et le contexte conservé en file. Elles concernent deux cartes proches
sur le profil indiqué ; elles ne constituent pas un essai de portée ni une
validation de tous les chemins et profils radio. Le format natif sur l'air
reste inchangé. Un relais qui choisit ensuite le format texte retrouve les
limites d'attribution par empreinte du RXT texte.
