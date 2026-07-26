# AC6 cycle 199 — consommateurs des bornes et classification d'état

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`

Passe statique headless en lecture seule sur le projet Ghidra canonique.
Les observations utilisent `FindDirectCallsTo.java`, `DecompileAt.java` et
`DumpRange.java`. Aucun projet, binaire, artefact généré ou configuration n'a
été modifié.

## Appels du helper de bornes

Les appels directs à `0x82268b28` sont situés à `0x822520e4`, `0x82268c40`,
`0x8226a134`, `0x8226a16c` et `0x8226e2b4`.

`0x82268b28` reçoit quatre flottants et un objet de sortie. Il compare les
paires `(f1,f3)` et `(f2,f4)` et écrit les minimums/maximums dans les champs
`+0x28c`, `+0x294`, `+0x290` et `+0x298`. Ce contrat est confirmé, mais le
type métier de l'objet reste inconnu.

La fonction `0x82268c10` donne une première frontière structurée : si son
second argument est non nul et que son byte `+0xa6` n'est pas `2`, elle lit
`+0x28`, `+0x30`, `+0x34` et `+0x3c`, les transmet au helper de bornes, puis
stocke le pointeur d'objet en `sortie+0x26c`. Il s'agit d'un objet de données
avec bornes calculées et référence auxiliaire; aucun lien avion/caméra n'est
prouvé.

## Classifieur de record autour de `0x8226e2b4`

Le flux brut `0x8226e180..0x8226e3dc` permet de qualifier un second
consommateur, malgré l'absence de frontière de fonction complète dans Ghidra :

1. un pointeur de record est récupéré depuis `objet+0x268` ;
2. ses quatre flottants `record+0x00`, `+0x04`, `+0x08` et `+0x0c` sont
   transmis à `0x82268b28`, avec `r31` comme objet de sortie ;
3. le record fournit ensuite quatre autres valeurs aux offsets `+0x10`,
   `+0x14`, `+0x18` et `+0x1c`, copiées dans `objet+0x30`, `+0x34`, `+0x38`
   et `+0x3c` ;
4. le byte `record+0x43` contrôle si cette copie est valide ;
5. des comparaisons flottantes classent l'objet dans les états `1`, `2`, `3`
   ou `4`, écrits à `objet+0x44` ;
6. une table de sauts sélectionne ensuite le traitement de l'état.

Le même flux initialise aussi `objet+0x40` selon le résultat d'un helper de
mode et stocke un flottant de record dans un tableau indexé à partir de
`objet+0x2c`. Les écritures suivantes utilisent des bornes `+0x28c` et
`+0x298`, ainsi que des champs `+0x48`, `+0x4c`, `+0x50` et `+0x54`.

Qualification sûre : objet de record/intervalle avec classification d'état et
bornes flottantes. Les constantes et offsets ne permettent pas de l'appeler
`AircraftBounds`, `AltitudeRange`, `CameraLimits` ou `FlightState`.

## Relation avec le contexte d'interpolation

Les appels `0x8226a134` et `0x8226a16c` apparaissent après l'initialisation du
contexte global par `0x822aaeb8` dans `0x8226a088`. Ils consomment toutefois
les champs d'un objet auxiliaire (`+0x270`/`+0x274`), pas directement les
quatre composantes de `0x823fb360`. Il faut donc conserver deux niveaux :

- contexte global de transition `0x823fb360` ;
- objet local de record/intervalle possédant ses propres valeurs et bornes.

Cette séparation évite de propager à tort une structure d'interpolation à tous
les objets qui utilisent les mêmes offsets relatifs.

## Statut et intervention humaine

- `confirmed` : appels du helper, calcul min/max, offsets de sortie,
  classification par `+0x44`, copie conditionnelle des champs du record ;
- `cross-match` : système d'intervalle/transition ou de paramètres de scène ;
- `unknown` / `needs-dynamic-evidence` : sémantique gameplay, propriétaire
  avion, caméra ou contrôleur de vol.

Aucun blocage nécessitant une action humaine n'est rencontré. Une trace Xenia
ne serait requise que pour relier les états `1..4` à une scène observable.

## Validation

- `FindDirectCallsTo.java 0x82268b28` ;
- `DecompileAt.java 0x82268c10` et `0x82268b28` ;
- `DumpRange.java 0x82251f40 0x82252140` ;
- `DumpRange.java 0x8226e180 0x8226e3e0` ;
- CTest AC6 : gate connu `41/41` ;
- launcher Xenia/Wine : `status=ready`, `release=16e1eb8`,
  `renderer=vulkan`.

