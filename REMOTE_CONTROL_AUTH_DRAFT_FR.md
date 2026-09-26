# Authentification des commandes distantes en LoRa APRS — brouillon

Statut : v1 en cours d'implémentation, **désactivée tant qu'aucune clé n'est installée**. Il s'agit
d'un protocole applicatif distinct transporté par messages LoRa APRS. Il ne
modifie ni le protocole LoRa APRS JSON ni RXT.

## Périmètre v1 validé — principe KISS

La première version reste volontairement minimale : une clé secrète aléatoire
par iGate, un seul contrôleur Graywolf, un compteur persistant, une commande
lisible en clair et un HMAC calculé par les bibliothèques cryptographiques
standard. Elle protège uniquement `?EM=ON/OFF`, `?TX=ON/OFF` et `?COMMIT`.
Dès son activation, ces commandes d'écriture non signées sont refusées.

La v1 ne comporte ni comptes multiples, ni certificats, ni synchronisation
temporelle, ni partage d'une clé entre plusieurs contrôleurs. La rotation
avancée des clés et la gestion multi-contrôleur restent hors périmètre.

## Fonctionnement actuel et périmètre

`src/query_utils.cpp` accepte les commandes privilégiées `?EM=ON/OFF`,
`?TX=ON/OFF` et `?COMMIT` si l'indicatif apparent de l'émetteur figure dans la
liste des gestionnaires. `src/station_utils.cpp` autorise aussi le joker final
`*` dans cette liste. Le réglage « RF uniquement » limite le transport, mais
n'empêche pas l'usurpation d'indicatif. Un ACK APRS confirme le transport, pas
l'authentification ni l'exécution de la commande.

La première étape devrait protéger ces commandes qui modifient l'état de
l'iGate. Les requêtes publiques en lecture seule (`?APRSV`, `?APRSP`,
`?APRSL`, `?APRSSR`) peuvent rester inchangées. Aucun mécanisme nouveau ne
doit être activé dans le firmware avant d'avoir validé le format, les vecteurs
de test, la persistance et la récupération.

## Propriétés de sécurité nécessaires

1. Le récepteur n'exécute qu'une commande destinée à son identité de contrôle
   **exacte**. Une copie adressée à un autre équipement échoue.
2. L'identité du contrôleur et de la cible, la version, l'identifiant de clé,
   le compteur et les **octets exacts de la commande** sont authentifiés
   ensemble. Le chemin TNC2, les marques de relais et les q-constructs
   APRS-IS peuvent changer en transit : ils ne sont pas signés.
3. Une trame enregistrée ne peut pas réexécuter sa commande, même après un
   redémarrage ou si plusieurs chemins RF en apportent des copies.
4. Une signature incorrecte, une clé inconnue, un ancien compteur, un message
   mal formé ou une action non autorisée ne changent rien. L'authentification
   précède l'appel aux traitements de commandes existants.
5. L'état anti-rejeu est enregistré **avant** l'exécution. Une coupure peut
   consommer un compteur sans exécuter la commande, mais ne doit jamais
   permettre de l'exécuter deux fois. Le contrôleur peut interroger l'état
   puis réessayer avec un nouveau compteur.
6. L'ACK APRS et le résultat applicatif authentifié sont distincts. Un ACK ou
   une mise en file TNC2 dans Graywolf ne prouvent pas la réussite de l'action.

## Mécanisme proposé

Utiliser une clé secrète propre à l'iGate et à son unique contrôleur Graywolf,
ainsi qu'un compteur strictement croissant. Calculer un code d'authentification sur une
enveloppe versionnée, définie octet par octet et contenant les champs du
point 2. HMAC-SHA-256 est le premier candidat ; la longueur de la signature
transmise et son encodage restent à fixer après examen de la sécurité et du
temps d'occupation radio. Il ne s'agit **pas** d'ajouter un simple code HOTP
à six chiffres : celui-ci ne serait pas lié à l'action demandée. Le compteur
évite de dépendre d'une horloge, tandis que le HMAC protège toute la commande.

### Format radio v1

Le corps du message APRS est :

```text
!RC1:A:<compteur-base36>:<commande>:<tag-base64url>
```

`A` est l'identifiant de clé v1. Le compteur est un entier non nul sur 64 bits,
en base 36 majuscule sans zéro initial. Les seules commandes admises sont
`EM=ON`, `EM=OFF`, `TX=ON`, `TX=OFF` et `COMMIT`. Le tag contient les 12
premiers octets de HMAC-SHA-256, encodés en Base64URL sans remplissage, soit
exactement 16 caractères.

Les octets authentifiés sont concaténés dans cet ordre :

```text
"LORA-APRS-RC"
0x01
uint16_be(longueur contrôleur) || contrôleur exact
uint16_be(longueur cible)      || cible exacte
"A"
uint64_be(compteur)
uint16_be(longueur commande)   || commande exacte
```

Les chaînes sont leurs octets ASCII transmis, sans terminateur. Le chemin APRS,
les marques `*`, les q-constructs et le numéro de message APRS `{nnn` ne sont
pas signés, car ils peuvent changer en transit.

Vecteur v1 : clé Base64URL
`AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8`, contrôleur `F4MLV-2`, cible
`F4MLV-10`, compteur 71 (`1Z`), commande `TX=OFF`, tag
`OLkaJxyKIXBM9SrF`. Le corps complet est :

```text
!RC1:A:1Z:TX=OFF:OLkaJxyKIXBM9SrF
```

Conserver sur chaque équipement la clé et le plus grand compteur accepté.
Les compteurs ne doivent ni boucler ni repartir à zéro
après redémarrage. L'émetteur enregistre son prochain compteur avant l'envoi ;
le récepteur enregistre le compteur accepté avant d'agir. Éviter une large
fenêtre d'anticipation et ne pas partager la clé avec une autre application
d'opérateur.

Une copie peut recevoir la réponse « déjà acceptée », mais ne doit jamais
réexécuter la commande. Le résultat applicatif devrait identifier la demande
et distinguer « exécutée », « refusée » et « déjà acceptée ». Si le
contrôleur doit se fier à cette réponse, elle doit aussi être authentifiée.
La persistance et l'authentification des résultats restent à concevoir ; un
ACK APRS ordinaire ne les remplace pas.

## Déploiement et compatibilité

- Installer les secrets par une voie locale ou administrative de confiance,
  jamais par commande RF en clair. Ne pas les exposer dans les sauvegardes
  JSON, les journaux, les réponses GET de la WebUI, APRS-IS, RXT ou les
  diagnostics. Prévoir le remplacement et la récupération des clés.
- Désactiver les commandes protégées tant qu'aucune clé n'est installée.
  Décider si les anciennes commandes fondées sur le seul indicatif seront
  ensuite refusées : accepter les deux annulerait la protection.
- Maintenir la réception JSON sans émission automatique. Un événement JSON,
  même représentable en AX.25, n'autorise jamais une commande de gestion.
- Graywolf peut être le premier contrôleur grâce à son compositeur de messages
  et à son canal TX TNC2 autorisé, mais le format doit rester indépendant de
  Graywolf pour que d'autres applications puissent l'utiliser.
- Le destinataire d'un message APRS classique est limité à neuf caractères.
  Limiter l'identité de contrôle à cette longueur en v1, ou définir un
  datagramme de contrôle TNC2 distinct. Toutes les identités étendues ne
  tiennent pas dans le champ APRS historique.
- Vérifier les règles applicables au trafic de contrôle authentifié sur les
  bandes radioamateur avant le déploiement. Ce brouillon ne se prononce pas
  sur toutes les juridictions ni sur tous les codages envisagés.

## Tests nécessaires avant activation sur RF

Vecteurs indépendants émetteur/récepteur ; modification de l'action, de la
cible, de la source ou du compteur ; mauvaise clé ; copies reçues par deux
iGates ; livraison dans le désordre ; redémarrage à chaque étape de
persistance ; champs mal formés ou trop longs ; absence d'exécution sur le
seul ACK APRS ; résultats explicites ; comparaison exacte des identités
étendues ; taille et temps d'occupation maximaux selon le profil radio. Un
essai matériel doit confirmer qu'une commande n'est jamais exécutée deux
fois après réémission ou coupure d'alimentation.

## Décisions encore ouvertes après la v1

1. Retour, déduplication et affichage des résultats authentifiés.
2. Récupération d'un site inaccessible et rotation avancée des clés.
3. Migration de la liste actuelle des gestionnaires et du réglage « RF
   uniquement ».

Références : [RFC 4226 (HOTP)](https://www.rfc-editor.org/rfc/rfc4226) et
[RFC 2104 (HMAC)](https://www.rfc-editor.org/rfc/rfc2104).
