# OTA radio : reprise après redémarrage du relais

## Objet et périmètre

Vérifier qu'un redémarrage du relais intermédiaire, avec perte de son cache
RAM, permet au contrôleur de retrouver la cible et de poursuivre depuis
les pages déjà enregistrées. Les trois essais sont chacun bornés à quatre
pages ; un quatrième ajoute une coupure physique sur la cible. Ils ne répètent pas le
[transfert complet du 30 octobre](../2026-10-30/OTA_LORA_fr.md).

## Premier essai : versions et montage

- Branche `experiment/lora-ota`, base `3dc297c`, avec les ajouts locaux du
  31 octobre : diagnostic relais 21 et option `--exercise-relay-restart`.
- MLV-2 : pont radio ; MLV-10 : relais unique ; MLV-15 : cible OTA-A,
  Wi-Fi et point d'accès désactivés, passage obligatoire par le relais.
- Profil initial : 434,100 MHz, BW125/SF12/CR4/5. Profil temporaire :
  434,500 MHz, BW500/SF7/CR4/5. Puissance de banc : 2 dBm ; radios SX1278.
- Image cible complète utilisée comme manifeste : 1 764 880 octets,
  SHA-256 `33fc1db7edf03dc80499cb93c0cd2f96579167029fbfc05b14b59c884a183a5d`.
- Firmware relais : SHA-256 du binaire
  `3b82b67ad889982971ade2704de2bdda9af72d28535d7cdd07ef3d64710f479d`.

## Premier essai : observations

1. La cible confirme par radio qu'aucune interface IP n'est active.
   Les deux premières pages sont enregistrées : 8 192 octets durables.
2. Le contrôleur envoie les 16 premiers blocs de la page suivante,
   destinés au cache du relais, puis la commande de redémarrage du relais.
   Aucun bilan de page n'a encore déclenché leur transmission à la cible.
3. L'état HTTP du relais confirme un changement d'identifiant de démarrage,
   un cache vide et le retour au profil initial. Cette lecture est une
   observation ; elle n'intervient pas dans la récupération radio.
4. Après deux demandes de bilan sans réponse, le contrôleur attend le
   retour autonome des profils, ouvre une nouvelle session authentifiée
   et retrouve exactement 8 192 octets durables. Il poursuit ensuite
   jusqu'à 16 384 octets. Les pages précédentes ne sont pas retransmises.
5. Le délai entre la commande de redémarrage et la reprise confirmée est
   de 115,14 secondes, dont 73,25 secondes pour la récupération après
   détection de la perte du lien. Ce sont les délais de ce client de banc.
6. La cible refuse WINDOW_FINISH et WINDOW_ACTIVATE sur cette image
   incomplète, avec le statut ORDER (2). L'application exécutée et la
   partition sélectionnée restent celles d'avant le transfert ; aucune
   confirmation de nouvelle application n'est en attente.
7. Après arrêt du transfert, une lecture indépendante de la partition
   inactive confirme que les 16 384 octets écrits sont identiques au début
   du fichier cible. SHA-256 de ce préfixe :
   `59f2775ed05a17c55d0b7ab08ab9aedc6711d2a205bb51373335743668381bba`.

## Deux essais complémentaires : groupe interrompu et acquittement perdu

Le même montage et les mêmes profils sont utilisés. Le firmware relais
ajoute l'opération de diagnostic 22, liée au nonce actif et à la page
sélectionnée. Le banc confirme son armement par HTTP avant l'envoi du groupe.
Ce contrôle et la lecture des témoins sont des observations de diagnostic ;
la récupération du transfert utilise uniquement la radio.

Le relais conserve avant le reset un témoin indiquant le mode d'injection,
la page et le nombre de blocs transmis ou l'offset durable annoncé par la
cible. Un nouvel identifiant de démarrage et la lecture de ce témoin
confirment ensuite l'injection effective.

| Panne provoquée sur la troisième page | Témoin après redémarrage | Progression retrouvée | Envoi après reprise |
|---|---|---|---|
| Après 16 blocs transmis vers la cible, entre deux trames du groupe | Mode 1, base 8 192, 16 blocs émis | 8 192 octets | Page interrompue, puis quatrième page : 64 blocs |
| Après réception du bilan complet de la cible, avant sa retransmission au contrôleur | Mode 2, base 8 192, offset durable 12 288 | 12 288 octets | Quatrième page seulement : 32 blocs et deux répétitions d'un bloc manquant |

Dans chaque cas, une nouvelle session authentifiée retrouve la progression
attendue puis atteint 16 384 octets. Les traces du contrôleur confirment
qu'aucun bloc des pages durables précédentes n'est renvoyé. Dans le second
cas, la troisième page n'est donc pas retransmise malgré la perte de son
acquittement. La récupération après détection de la perte du lien prend
respectivement 73,23 et 73,24 secondes, avec les délais du client de banc.

Les deux essais vérifient à nouveau le refus de WINDOW_FINISH et
WINDOW_ACTIVATE sur l'image incomplète, le maintien de l'application et
de la partition de démarrage, et l'absence d'interface IP active sur la cible.
Après chaque session, une lecture indépendante de la partition inactive
confirme l'identité des 16 384 octets avec le fichier utilisé.

Les fichiers A et B ont volontairement des préfixes différents, pour que
la seconde lecture ne puisse pas seulement retrouver les octets du premier
essai. Chacun représente une image complète de 1 764 880 octets, mais seul
son début est transmis :

- Groupe interrompu, image B : SHA-256
  `e0b39c4b97080ad9fcb533e371d2d48fbe3419a5836a9c7cd2d41d0b5736270b`.
- Acquittement perdu, image A : SHA-256
  `34ac4783af9e86ef350c6df92af6606316fd523c0726b96993702fb76be3012a`.
- Relais utilisé pour les deux essais : 1 754 400 octets, SHA-256
  `9629145c426dfcf6faf1c5d5661fd926c3b6fe58f3491ccac78d4061e500ac73`.

Le code reste basé sur `3dc297c` avec les modifications locales de diagnostic
et de client du 31 octobre. Les preuves des deux essais complémentaires
sont conservées dans `logs/lora-ota-relay-phases-20261031/`.

## Coupure physique avec une page partiellement écrite

Le montage reste MLV-2 → MLV-10 → MLV-15, sur les mêmes profils radio.
La cible porte une variante de diagnostic ajoutant les opérations 23 et 24.
L'opérateur confirme que MLV-15 est alimenté uniquement par son câble USB.

1. Deux pages sont enregistrées normalement : 8 192 octets durables.
   Le diagnostic arme la troisième page. Quand tous ses blocs sont reçus,
   la cible efface ses 4 096 octets, écrit seulement les 2 048 premiers et
   suspend les écritures, sans avancer le journal. La réponse radio signée
   confirme ces valeurs et la pause. Le profil radio initial est rétabli
   sans annuler le journal de reprise.
2. L'opérateur débranche puis rebranche l'USB. Un observateur passif,
   sans ouverture du port série, enregistre sa disparition puis son retour,
   espacés de 22,22 secondes.
3. Avant toute reprise, une lecture indépendante de la troisième page
   confirme les 2 048 premiers octets identiques au fichier cible et les
   2 048 suivants effacés (`FF`). Cette lecture provoque ensuite un reset
   supplémentaire ; l'essai ne prétend donc pas observer un redémarrage
   unique après la coupure.
4. Une nouvelle session radio authentifiée confirme la disparition de la
   pause volatile puis retrouve le journal à 8 192 octets. Le contrôleur
   envoie les 64 blocs des troisième et quatrième pages, sans retransmettre
   les pages précédentes, et atteint 16 384 octets durables.
5. La cible refuse de finaliser ou d'activer cette image incomplète. Son
   application et sa partition de démarrage restent inchangées ; le Wi-Fi
   reste désactivé. Une lecture indépendante des 16 384 octets confirme
   leur identité avec le fichier d'origine, y compris la page réécrite.

Image B utilisée comme manifeste : 1 765 280 octets, SHA-256
`3ad9c0f04f7e767c641268eb9eacf8c795baf2b6313fa34441dcbe7bfcd26721`.
SHA-256 du préfixe vérifié après reprise :
`75ba1ede8a10b0611bf3b4f2374c09146cb5e5e952a3d79b6deb4485b6345b4b`.
Les sources restent basées sur `3dc297c` avec les modifications locales de
diagnostic du 31 octobre. Les artefacts, journaux et preuves sont conservés
dans `logs/lora-ota-power-cut-20261031/`.

Cet essai établit une reprise après coupure réelle avec une page incomplète
en flash et un journal intact. L'arrêt de diagnostic intervient entre deux
opérations de programmation : il ne valide pas une coupure au milieu d'un
appel matériel de programmation, d'effacement ou d'écriture du journal NVS.

## Conclusion et limites

Cet essai montre une reprise automatique par radio après un redémarrage
du relais, avec conservation des pages durables sur la cible, perte du
cache intermédiaire et retransmission de la page interrompue. Les essais
complémentaires couvrent aussi l'interruption du groupe en cours d'envoi
et la perte de l'acquittement d'une page déjà enregistrée. Le refus d'activer
l'image incomplète et l'exactitude du préfixe écrit sont vérifiés dans les
trois cas. Le quatrième ajoute une coupure réelle d'alimentation avec une
page partielle, suivie de sa réécriture correcte.

Il ne valide pas un transfert complet supplémentaire, une coupure
électrique pendant une opération matérielle d'écriture ou d'effacement,
plusieurs relais, des chemins alternatifs
ni la portée en conditions réelles. Chaque scénario a été testé une fois
avec une injection effectivement observée. Les essais ne constituent pas
une exploration de tous les instants de panne possibles. Pour le premier
essai, les 16 blocs ont été envoyés vers le cache ; leur présence individuelle
dans ce cache juste avant le reset n'a pas été lue. Le reset pendant le
groupe intervient entre deux trames, sans coupure au milieu d'une émission.

Les journaux, le patch exact, les images et les preuves de lecture restent
dans `logs/lora-ota-relay-reset-20261031/`, hors publication. Les tests sur
ordinateur passent : 19 tests Python et les suites C++ session, profils,
pages et enveloppes. Les variantes OTA-A, OTA-B, relais et standard compilent.
