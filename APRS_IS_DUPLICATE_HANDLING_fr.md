# Déduplication APRS-IS et affichage aprs.fi

Cette note rassemble les règles générales à conserver pour interpréter les
doublons APRS-IS et l'affichage des paquets bruts par aprs.fi. Ce ne sont pas
des résultats propres à un essai du firmware.

## Déduplication effectuée par APRS-IS

APRS-IS recherche les doublons dans une fenêtre glissante de 30 secondes. La
comparaison porte sur la source, la destination et le champ de données du
paquet. Le chemin de transport APRS n'entre pas dans cette comparaison.

Il en résulte que :

- deux copies qui ne diffèrent que par leur chemin RF/APRS-IS doivent être
  reconnues comme le même paquet si elles arrivent dans cette fenêtre ;
- une copie retardée de plus de 30 secondes peut traverser le réseau ;
- la moindre différence dans le champ de données peut empêcher la
  déduplication, même si les deux paquets représentent le même événement pour
  l'utilisateur ;
- dans une trame APRS tierce (`}`), l'en-tête et le chemin du paquet encapsulé
  appartiennent au champ de données de la trame extérieure. Une modification de
  cette partie n'est donc pas une simple différence de chemin extérieur.

La déduplication APRS-IS est un filtre borné. Elle ne garantit pas qu'une seule
copie d'un événement sera visible de bout en bout et elle ne constitue pas un
mécanisme de coordination ou d'élection entre plusieurs iGates.

## Signification du rouge sur aprs.fi

Dans la vue des paquets bruts d'aprs.fi, le rouge ne désigne pas un doublon.
Il indique un paquet qu'aprs.fi considère comme invalide, ou un paquet valide
dont le type n'est pas pris en charge. Le texte d'erreur affiché avec la ligne
donne la raison retenue par aprs.fi.

Deux lignes rouges qui semblent dupliquées établissent donc deux faits
distincts :

1. les deux paquets sont parvenus jusqu'à aprs.fi ;
2. aprs.fi a signalé séparément un problème de validité ou de prise en charge
   pour chacun d'eux.

La couleur rouge ne permet pas, à elle seule, de déterminer pourquoi les deux
copies ont échappé à la déduplication APRS-IS.

## Méthode d'analyse d'un doublon observé

Avant de conclure, conserver et comparer :

- les deux lignes TNC-2 complètes, sans les normaliser ;
- leurs horodatages et leur écart réel ;
- le texte d'erreur associé à chacune par aprs.fi ;
- les octets du champ de données lorsqu'une différence invisible est possible.

Si les arrivées sont espacées de plus de 30 secondes, la fenêtre APRS-IS suffit
à expliquer leur présence. Si elles sont plus rapprochées, rechercher une
différence de contenu : suffixe RXT, espace, caractère non imprimable,
encapsulation tierce, transformation du paquet ou nouvelle retransmission de
message. Une différence limitée au chemin extérieur ne suffit normalement pas
à expliquer le passage des deux copies.

Le cas RXT documenté dans `RXT_APRSIS_BOUNDARY_fr.md` est un exemple précis :
le suffixe ajouté au champ de données rend les deux copies différentes pour
APRS-IS. Il ne faut pas généraliser cet exemple à tous les doublons.

## Sources de référence

- APRS-IS, *Server Design* : <https://aprs-is.net/ServerDesign.aspx>
- aprs.fi, *Raw packets* : <https://aprs.fi/doc/guide/aprsfi-raw-packets.html>
- aprs.fi, *On duplicate and delayed packets* :
  <https://blog.aprs.fi/2008/03/on-duplicate-and-delayed-packets.html>

