# AC6 cycle 197 — constructeur du manager et contrat des slots

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`

Passe statique headless, en lecture seule, sur le projet Ghidra canonique.
Les scripts utilisés sont `FindSymbolReferences.java`, `DecompileAt.java`,
`DumpU32Range.java` et `DumpRange.java`. Aucun binaire, projet Ghidra, fichier
généré ou configuration runtime n'a été modifié.

## Publication du singleton

Le constructeur/initialiseur `0x82183c80` ne laisse plus
`DAT_829188a0` comme un simple pointeur opaque :

- il construit l'objet principal à `0x829d15a0` via `0x82183960` ;
- il construit un autre objet de service à `0x829d3870` via `0x821835d8` ;
- il publie `DAT_8291889c = 0x829cc318` ;
- il publie `DAT_829188a0 = 0x829d15a0` ;
- il publie `DAT_829188a8 = 0x829d3870` ;
- il efface ensuite une zone d'état de l'objet publié.

`0x82183960` installe la vtable primaire `0x82062f34`, une vtable imbriquée
à l'offset `+0x1810` environ et plusieurs sous-objets. Il initialise aussi une
grande zone de blocs flottants et des pointeurs de service. `0x821835d8`
initialise quinze enregistrements imbriqués avec la vtable `0x8205d7f0`.

La qualification sûre est donc un grand objet de service/état événementiel,
ressource et transition. La chaîne ASCII `EventMovie` voisine dans la zone de
vtable corrobore un sous-système événementiel, mais ne justifie pas un nom de
gameplay plus précis.

## Vtable primaire et slots résolus

La vtable `0x82062f34` donne les entrées suivantes :

| slot | cible | contrat statique |
|---|---:|---|
| `+0x48` | `0x82183b20` | écrit `f1` dans `manager+0x1808`, puis retourne |
| `+0x4c` | `0x82183b28` | écrit `f1` dans `manager+0x180c`, puis retourne |
| `+0x5c` | `0x822ddbe8` | no-op explicite dans cette vtable |
| `+0x64` | `0x8212f290` | réinitialise le quadruplet global `0x823fb360+0x04..+0x10` avec la constante `0x820542b8` |
| `+0xb0` | `0x82184d88` | fragment de prédicat/dispatch sur les champs `+0x1aec`, `+0x1af0`, `+0x1af4` d'un objet reçu |

Les deux setters flottants sont désormais `confirmed` au niveau ABI et offset.
Ils ne doivent toutefois pas être renommés `SetPosition`, `SetHeading` ou
`SetSpeed` : aucune preuve ne donne encore la sémantique de
`manager+0x1808/+0x180c`.

Le slot `+0x64` est également résolu au niveau instructions : il copie une
constante flottante dans les quatre composantes de l'état global utilisé par
le cas dispatcher `0x3001`. Cela renforce une interprétation de reset/état de
transition, sans prouver qu'il s'agit d'une pose d'avion.

## Appelants et correction de portée

`0x821862c8` sélectionne le slot `+0x5c` ou `+0x64` selon les champs d'état
`+0x13700`, `+0x13704` et `+0x136fc`. `0x82184500` appelle aussi `+0x64` dans
une branche de sa machine d'état. Cela confirme que ces slots sont des
callbacks de service/transition du singleton publié.

Le cas `0x3035` de `0x8212b8ac` appelle bien les slots `+0x48` et `+0x4c` du
manager global avec les flottants du record (`record+0x18`, puis
`record+0x14`).

Correction importante du cycle 196 : les cas `0x3033/0x3034` chargent un
objet de table depuis `0x03006054` avant d'appeler son slot `+0xb0`. Il ne faut
pas décrire cet appel comme un slot `+0xb0` du manager global
`DAT_829188a0`. Le manager possède bien une entrée `+0xb0` à
`0x82184d88`, mais la réception des cas `0x3033/0x3034` est une autre
frontière d'objet.

## Statut et limites

- `confirmed` : constructeur et publication de `DAT_829188a0`, objet
  `0x829d15a0`, vtable `0x82062f34`, setters `+0x48/+0x4c`, no-op `+0x5c`,
  reset quadruplet `+0x64`, séparation du receiver `+0xb0` des cas
  `0x3033/0x3034` ;
- `cross-match` : rôle de service/transition et relation avec le pipeline de
  records/mouvement ;
- `unknown` / `needs-dynamic-evidence` : identité gameplay des champs,
  relation à l'avion joueur, caméra, trajectoire ou contrôleur de vol.

Ne pas introduire les noms `PlayerAircraft`, `CameraController`, `FlightState`
ou `SetPosition`. La passe statique a réduit l'incertitude sur la frontière
du manager, mais ne justifie pas encore une validation dynamique du gameplay.
Aucune action humaine n'est requise pour ce cycle ; une trace ne devient
nécessaire que si l'on veut attribuer une sémantique gameplay aux champs.

## Validation

- `analyzeHeadless ... -readOnly -noanalysis ... FindSymbolReferences.java 829188a0`
- `DecompileAt.java` sur `0x82183c80`, `0x82183960`, `0x821835d8`,
  `0x821862c8` et `0x82184500` ;
- `DumpU32Range.java 0x82062f34 0x820631b4` ;
- `DumpRange.java 0x8212f1e0 0x8212f3c0` et
  `DumpRange.java 0x82183a00 0x82183f00` ;
- CTest AC6 : 41/41 dans le dernier gate ;
- launcher Xenia/Wine : `status=ready`, `release=16e1eb8`,
  `renderer=vulkan` dans le dernier gate ;
- `git diff --check` sans erreur après la documentation.

