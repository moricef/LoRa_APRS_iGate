# Documentation du fork LoRa APRS iGate

Les références techniques sont conservées à la racine pour maintenir leurs
adresses publiques. Les essais datés, propositions en cours, présentations
et archives sont classés séparément. Les anciens chemins publics des
documents déplacés contiennent un lien vers leur nouvel emplacement.

## Références techniques

| Sujet | Document | Statut |
|---|---|---|
| RXT | [RXT.md](RXT.md) | Transport et comportement du firmware ; point d’entrée pour RXT |
| Format natif : objectifs | [Français](LORA_APRS_NATIVE_FORMAT_fr.md) · [English](LORA_APRS_NATIVE_FORMAT.md) | Architecture proposée, distincte du périmètre déjà implémenté |
| Format natif : octets | [Français](LORA_APRS_NATIVE_FRAME_DRAFT0_fr.md) · [English](LORA_APRS_NATIVE_FRAME_DRAFT0.md) | Brouillon 0 implémenté et testé ; format expérimental |
| Routage retour des messages | [Français](REVERSE_MESSAGE_ROUTING_fr.md) · [English](REVERSE_MESSAGE_ROUTING.md) | Fonctionnement et limites du chemin retour appris |
| Doublons APRS-IS et aprs.fi | [Note de référence](APRS_IS_DUPLICATE_HANDLING_fr.md) | Règles à relire avant toute analyse de doublons |
| Référence APRS-IS | [Markdown](APRS_IS_REFERENCE.md) · [PDF](APRS_IS_REFERENCE.pdf) | Référence complémentaire |
| Authentification distante | [Français](REMOTE_CONTROL_AUTH_DRAFT_FR.md) · [English](REMOTE_CONTROL_AUTH_DRAFT.md) | Brouillon de protocole ; ne remplace pas le code ni le guide opérateur Graywolf |

## Propositions en discussion

[Format commun natif et PPM : variantes chiffrées](proposals/FORMAT_COMMUN_VARIANTES_fr.md)
· [English](proposals/COMMON_FORMAT_VARIANTS.md).

Ce travail compare les formats et le coût des codecs. Il ne constitue pas
une décision de format ni une validation de la chaîne complète de relayage.
Les tailles avec RXT sont des estimations distinctes des essais de codec.

## Comptes rendus d’essais

Chaque compte rendu est daté. Ses conclusions restent limitées aux versions,
configurations et observations qu’il décrit ; il ne fait pas office de
spécification actuelle.

| Date | Essais |
|---|---|
| 28 septembre 2026 | [Correctifs RF](tests/2026-09-28/VALIDATION_CORRECTIFS_RF_20260928_fr.md) · [English](tests/2026-09-28/RF_FIX_VALIDATION_20260928_en.md) |
| 29 septembre 2026 | [RXT v2](tests/2026-09-29/VALIDATION_RXT_V2_20260929_fr.md) · [English](tests/2026-09-29/RXT_V2_VALIDATION_20260929_en.md) |
| 30 septembre 2026 | [Natif et frontière APRS-IS](tests/2026-09-30/ESSAI_NATIF_ET_RXT_APRSIS_20260930_fr.md) · [English](tests/2026-09-30/NATIVE_TRIAL_AND_RXT_APRSIS_20260930_en.md) |
| 13 octobre 2026 | [RXT natif sur l’air](tests/2026-10-13/VALIDATION_RXT_NATIVE_RF_20261013_fr.md) · [English](tests/2026-10-13/VALIDATION_RXT_NATIVE_RF_20261013.md) |
| 26 octobre 2026 | [Filtre des types de trame et Bluetooth](tests/2026-10-26/VALIDATION_LORA_FRAME_BT_20261026_fr.md) : comparaison à la compilation, antérieure aux mesures de tas sur carte |
| 27 octobre 2026 | [Collisions entre relais](tests/2026-10-27/RELAY_COLLISIONS_20261027_fr.md) : expérimentation mise de côté ; branche d’archive indiquée dans le rapport |

Les captures, bancs, binaires et sauvegardes restent dans `logs/`, ignoré
par Git. Les journaux de l’essai Bluetooth sur carte du 26 octobre sont dans
`logs/bt-20261026/` ; les sauvegardes de configuration peuvent contenir des
secrets et ne doivent pas être publiées.

## Présentations et archives

- [Présentations du 15 octobre 2026](presentations/2026-10-15/) : sources
  Markdown, documents ODT, PDF et diapositives conservés ensemble. Ils
  décrivent l’état et les explications de cette date, pas le firmware courant.
- [Correspondances préparées pour Jon](archives/correspondence/2026-10-15/) :
  brouillons et exports conservés pour historique ; leur présence ne prouve
  pas qu’ils ont été envoyés.
- Proposition historique des tuples RXT v2 :
  [français](archives/proposals/RXT_V2_fr.md) ·
  [English](archives/proposals/RXT_V2.md). Le transport actuel est décrit
  dans [RXT.md](RXT.md).

## Notes locales et inventaire

`docs/local/`, ignoré par Git, contient les reprises historiques, les
investigations d’authentification et le diagnostic AnyDesk. La note BURNTX
du 24 octobre est signalée comme une piste abandonnée : le correctif
firmware qu’elle décrivait a été retiré.

L’inventaire de gestion reste à son chemin prescrit :
`docs/DEPLOYMENTS_fr.md`. Les consignes de reprise restent dans `AGENTS.md`.
Ces fichiers locaux et les secrets ne sont pas des documents à publier.

## Web Flasher

[Procédure de compilation et publication](flasher/README.md).

Le flasher conserve ses chemins : `index.html`, `manifest-*.json`,
`favicon.png` et `firmware/`. La réorganisation documentaire ne remplace
aucun binaire et ne modifie aucun manifeste.
