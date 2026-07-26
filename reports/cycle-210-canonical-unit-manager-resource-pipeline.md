# AC6 cycle 210 — canonical unit-manager resource/object pipeline

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet : `workspaces/ace-combat-6/ghidra-projects/ace-combat-6`
- mode : Ghidra headless, `-readOnly -noanalysis`.

Cette passe suit le consommateur immédiat de l'objet obtenu par la route DPL
`0x820a85e0`. Elle vise à distinguer la construction d'unités/ressources du
chargement d'une scène de campagne ou d'un objet de vol.

## RTTI et point d'entrée du manager

La vtable canonique à `0x82055190` est précédée par le COL `0x8206d024` à
`0x8205518c`. Le descripteur de type à `0x8268f288` contient le nom décoré
`.?AVCX360UnitManager@@`; les descripteurs de hiérarchie voisins sont ceux de
la même famille RTTI. Cette revalidation porte sur le manager lui-même. Elle ne
transpose pas automatiquement les sous-types déduits dans d'anciens rapports
ou dans le projet historique `ace-combat-6-corrected`.

Le slot `+0x0c` de la vtable pointe vers `0x820a85e0`. Le corps de
`0x820a7070`, appelé par les entrées de gestion correspondantes, récupère le
premier enfant DPL, parcourt 94 éléments, puis transmet chaque élément aux
services virtuels `+0x18` et `+0x1c` avec le littéral de type `0x98`. Les
sélecteurs de source `1..6` choisissent ensuite plusieurs chemins de
construction et d'initialisation.

Qualification : `confirmed` pour la vtable, le RTTI du manager, le slot DPL,
la borne de 94 éléments et les appels de service typés `0x98`.

## Objet post-DPL et ressources secondaires

À `0x820a7a08`, le manager appelle `0x820a8678` avec l'objet construit et des
contextes de source. Le corps de `0x820a8678` :

- demande plusieurs identifiants dépendant de l'état via `0x821b6fb0`,
  `0x821b7038` et `0x821b7108`;
- reformate des clés DPL par `0x821d1060`;
- résout les ressources avec `0x821d2fc0`, `0x82234dd0` et le service
  `0x820a7eb0`;
- lit des enfants DPL aux indices `0`, `1`, `2`, `3` et `6`, ainsi que des
  indices dérivés des octets de source `+0x61/+0x62`;
- écrit notamment la ressource obtenue à `objet + 0x15c`;
- invoque des slots de service `+0x10`, `+0x18` et `+0x1c`, puis prépare des
  données vectorielles et des champs de l'objet autour de `+0x228`, `+0x248`,
  `+0x250`, `+0x25c` et `+0x274`.

Les branches brutes `0x820a793c -> 0x820a8bb8` et
`0x820a8120 -> 0x820a8e08` montrent d'autres chemins de construction ou de
ressource. Elles ne suffisent pas à leur attribuer une fonction aéronef,
spawn, mission ou position.

Les helpers `0x821b6fb0`, `0x821b7038` et `0x821b7108` restent des fonctions de
table/ID dépendant de l'état. Aucun texte de campagne, identifiant de mission,
groupe Scene, `CutTerminate`, receiver `DAT_8293BA10+8` ou objet de vol n'est
référencé dans cette tranche.

Qualification : `confirmed` pour le pipeline manager → DPL → ressource et pour
les écritures d'offsets observées; `unknown` / `needs-types` pour les types
concrets des objets retournés et la sémantique gameplay des champs; aucun lien
post-CUT n'est établi.

## Conclusion et frontière

Le chaînon immédiat après `0x820a85e0` est un pipeline de
`CX360UnitManager` qui assemble des objets et leurs ressources DPL. Il resserre
la recherche vers un consommateur typé de l'objet manager ou vers l'un des
services appelés après l'écriture `+0x15c`; il ne ferme pas le verrou
post-CUT.

Les trois joints nécessaires restent :

1. l'affectation et l'implémentation concrète du receiver
   `DAT_8293BA10 + 8`;
2. le mapping campagne du selector `1` vers un groupe Scene précis;
3. un consommateur typé de `CutTerminate` qui prouve la transition suivante.

AC6 reste `native-partial`, avec une frontière native à `scene_complete`. Cette
tranche ne demande aucune action humaine : la prochaine récupération peut
rester statique et ciblée.

## Validation

- revalidation canonique de la vtable `0x82055190`, du COL `0x8206d024` et du
  type descriptor `0x8268f288`;
- `DumpRange.java` sur `0x820a7600..0x820a7b40`,
  `0x820a8600..0x820a8d80` et `0x820a8d7c..0x820a8f80`;
- `DumpRange.java` sur `0x821b6ee0..0x821b7160`;
- `FindPpcRawBranchesTo.java` pour `0x820a8678`, `0x820a8bb8` et
  `0x820a8e08`;
- toutes les commandes sur le projet canonique, en lecture seule, sans
  import du projet `ace-combat-6-corrected`.

Les portes locales déjà établies restent : CTest AC6 **41/41 PASS** et oracle
Xenia/Wine `status=ready`, release `16e1eb8`, renderer `vulkan`, service
`ac6-xenia-wine-gui.service`. Aucune session humaine n'est requise pour ce
résultat statique.
