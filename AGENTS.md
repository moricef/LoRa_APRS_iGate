# Reprise de contexte du dépôt

Après toute initialisation, réinitialisation ou perte de contexte, l'agent doit
se remettre au courant de l'état réel du dépôt avant de proposer ou effectuer
une modification.

Il doit au minimum :

1. vérifier le répertoire de travail et la branche courante ;
2. lire `git status --short --branch` ;
3. examiner les derniers commits pertinents avec `git log` ;
4. examiner les changements suivis avec `git diff` et `git diff --cached` ;
5. prendre connaissance des fichiers non suivis et des éventuels fichiers de
   reprise ou de documentation liés à la tâche en cours.

L'agent doit considérer les changements locaux existants comme appartenant à
l'utilisateur. Il ne doit ni les écraser, ni les annuler, ni repartir d'un état
supposé propre. Si l'accès au réseau est disponible et qu'une comparaison avec
le dépôt distant est nécessaire, il peut actualiser les références distantes
avant de conclure sur l'état de synchronisation.

Pour toute analyse de doublons APRS-IS, de paquets rouges sur aprs.fi ou de
coordination entre iGates, l'agent doit d'abord relire
`docs/APRS_IS_DUPLICATE_HANDLING_fr.md`. Cette note contient les règles de base
déjà établies et leurs sources ; elles ne doivent pas être redécouvertes à
chaque reprise de contexte.

Pour toute question ou opération concernant un équipement, un site, une
adresse réseau, un rôle radio ou un firmware déployé, l'agent doit d'abord lire
`docs/DEPLOYMENTS_fr.md`. Il doit distinguer les adresses de gestion, les
endpoints locaux d'intégration et les identités APRS. Il ne doit jamais
compléter un champ inconnu par analogie avec un autre équipement. Après un
flash ou une modification réseau confirmée, il doit mettre cet inventaire à
jour.

## Respect du périmètre demandé

Quand l'utilisateur fournit un texte à lire ou à analyser, l'agent doit se
limiter à cette lecture ou à cette analyse. Il ne doit pas rédiger spontanément
une réponse, un message, un commentaire, une issue ou une publication, ni
proposer un texte prêt à envoyer, sauf demande explicite de l'utilisateur.

L'analyse, la rédaction d'une réponse et sa publication sont trois tâches
distinctes. L'autorisation donnée pour l'une ne vaut pas autorisation pour les
autres. L'agent ne doit pas ajouter une étape simplement pour donner une
impression d'achèvement ou d'exhaustivité.

Dans un rapport de test, chaque élément conservé doit contribuer directement à
l'objectif du test ou borner explicitement la conclusion. Les opérations de
remise en état, les essais exploratoires remplacés par des essais concluants et
les observations sans valeur probante doivent être exclus. Toute conclusion
doit rester strictement limitée à ce que les observations démontrent.

## Fichiers TODO et de reprise

Quand l'utilisateur demande de créer un TODO, un plan de reprise ou un document
de passation, l'agent doit l'enregistrer dans un emplacement persistant du dépôt
concerné. Il ne doit jamais placer ce document ni le worktree qui le contient
dans `/tmp` ou dans un autre répertoire temporaire susceptible de disparaître
après un redémarrage.

Si la tâche concerne un autre dépôt, l'agent doit utiliser le répertoire durable
de ce dépôt ou créer un worktree durable à proximité de celui-ci. Avant de
terminer, il doit vérifier que le document est accessible par un chemin local
persistant. Un commit ou une publication distante ne remplace pas cette
vérification locale.

# Publication du Web Flasher et de `gh-pages`

La publication de l'iGate doit prendre comme modèle la procédure du Tracker,
documentée dans `../UPDATE_WORKFLOW.txt`. Elle doit rester reproductible et
rapide : l'agent ne doit pas recréer un worktree de publication jetable dans
`/tmp` à chaque mise à jour.

L'agent doit utiliser une branche locale `gh-pages` ou un checkout/worktree
durable situé à proximité du dépôt. Si le répertoire principal contient des
changements de l'utilisateur qui rendent un changement de branche risqué, il
doit employer ce worktree durable plutôt que déplacer, masquer ou écraser ces
changements.

La procédure de publication est la suivante :

1. compiler tous les variants concernés depuis la branche de développement ;
2. mettre à jour dans le même commit les firmwares de `docs/firmware/`, leurs
   `SHA256SUMS`, les manifestes et les informations visibles du flasher ; la
   date et l'heure UTC exactes de chaque build doivent correspondre au champ
   `Build date` intégré dans la WebUI du binaire concerné ;
3. pousser ce commit sur le remote `fork` ;
4. recopier le contenu publié de `docs/` dans le checkout durable de
   `gh-pages` ; pour ce dépôt, GitHub Pages sert ce contenu à la racine de la
   branche, et non dans un sous-répertoire `docs/` ;
5. committer et pousser `gh-pages` sur `fork` ;
6. attendre la réussite du déploiement GitHub Pages, puis vérifier les
   manifestes publics et comparer les SHA-256 des firmwares servis avec les
   binaires locaux.

La publication ne doit jamais écraser des fichiers sans rapport déjà présents
sur `gh-pages`. Les fichiers temporaires de téléchargement ou de vérification
peuvent utiliser `/tmp`, mais ni le checkout de publication ni une information
de reprise unique ne doivent en dépendre.
