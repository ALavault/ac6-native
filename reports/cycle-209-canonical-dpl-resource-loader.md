# AC6 cycle 209 — canonical DPL resource-loader boundary

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet : `workspaces/ace-combat-6/ghidra-projects/ace-combat-6`
- mode : Ghidra headless, `-readOnly -noanalysis`.

Cette passe suit le chaînon immédiatement postérieur à la consommation du
current-level par `0x820a85e0`. Elle vérifie si le dispatcher appelé sur la
route DPL constitue une activation de scène ou un service de chargement de
ressources.

## Route DPL canonique

La décompilation de `0x820a85e0` est précise : lorsque son second argument est
non nul, elle récupère le contexte global via `DAT_8293BA10 + 0x54`, lit le
current-level par `0x820943b0`, le convertit par `0x821b6e58`, puis appelle
`0x821d1060`. Cette dernière fabrique la chaîne avec le format situé à
`0x82067b00` :

```text
DPL:[#x,#x]
```

Le résultat passe ensuite par `0x821d2fc0`, puis par
`0x82234dd0(result + 0x20, 1)`, et est attaché au second objet via
`0x8228e988`.

La décompilation de `0x821d2fc0` confirme une recherche hiérarchique : elle
appelle `0x821d1be8` avec la clé et un champ de contexte, puis parcourt des
providers chaînés (`provider->vtable+0x0c`) avant de réessayer récursivement.
`0x82234dd0` est une simple lecture d'entrée indexée (borne contre
`*table`, puis `base + offsets[index]`). Ces deux helpers ne contiennent pas
de transition de mode.

Cette séquence établit un chargement/résolution d'objet DPL. Elle ne contient
ni événement `CutTerminate`, ni identifiant de mission, ni création d'un
joueur ou d'un objet de vol.

## Le dispatcher est un Resource Manager

`0x821d1128` n'a pas de caller direct ou de branche PPC brute vers son adresse
dans le `.text` canonique. La seule référence directe est la donnée
`0x82067b90`, dans une table de méthodes dont les entrées voisines sont
`0x821d1550`, `0x821d1620`, `0x821d1710` et `0x821d18e8`. La même zone contient
la chaîne ASCII `Resource Manager:%s` à `0x82067bf8`.

Le dump de `0x821d1128..0x821d154c` montre une machine d'état de ressource :

- elle lit l'état de l'objet à `this + 0x0c`;
- l'état `0` recherche une entrée dans des tables indexées, appelle un slot
  virtuel `+0x28`, puis `0x821cbfd0` et `0x82222e98`;
- l'état `1` résout une entrée et écrit notamment `this + 0x140`, `+0x144`,
  `+0x14c` et `+0x13c`;
- les états invalides ou hors plage renvoient `-1` et nettoient les champs
  temporaires;
- les transitions finales évaluent des bornes d'identifiant (`0x1e1` et
  `0x3c0`) et renvoient un statut.

Ce comportement est compatible avec un gestionnaire de ressources et non avec
un activateur de scène post-CUT. Aucun chargement de `DAT_8293BA10`, NFIC,
Scene group ou receiver partagé n'apparaît dans cette méthode.

## Helper d'attachement

`0x8228e988`, appelé par `0x820a85e0`, effectue uniquement une liaison de
pointeurs relatifs :

```text
out+0x00 = base
out+0x04 = base + *(base+0x0c)
out+0x08 = base + *(base+0x10)
```

Il ne déclenche aucun dispatch métier et ne publie pas le résultat dans le
propriétaire partagé `DAT_8293BA10`.

## Contrôle du signal propriétaire

La décompilation canonique de `0x821b6668` confirme que le signal partagé
`{owner+0x18 = 1, owner+0x1c = 3}` et le dispatch `owner+0x8` slot `+0x20`
restent une machine d'état générique. Elle appelle le receiver lorsque l'état
du task vaut `1`, puis peut décrémenter un compteur et republier le signal;
elle ne référence ni `0x820a85e0`, ni la chaîne DPL, ni NFIC.

## Qualification

- `confirmed` : `selector 1 -> DPL id 9`, format `DPL:[#x,#x]`, résolution par
  `0x821d1060`, méthode Resource Manager référencée par `0x82067b90`, et
  attachement relatif par `0x8228e988`;
- `cross-match` : structure de table de méthodes du Resource Manager et
  machine d'état de chargement observée dans `0x821d1128`;
- `unknown` / `needs-dynamic-evidence` : l'objet métier créé par
  `0x82234dd0`, son consommateur ultérieur, et tout lien vers une activation de
  Scene ou de vol.

La frontière post-CUT n'est donc pas fermée. AC6 reste `native-partial` et
doit rester borné à `scene_complete`. Aucune action humaine n'est requise pour
cette tranche : la prochaine cible reste la récupération statique du
consommateur de l'objet DPL ou du receiver partagé, sans déduire une mission à
partir du simple chargement de ressource.

## Validation

- `DecompileAt.java 0x820a85e0`, `0x821d1060`, `0x821b6668` et `0x821d1128`;
- `DumpRange.java 0x821d1128..0x821d1600`;
- `DumpU32Range.java 0x82067af0..0x82067b30` et
  `0x82067b40..0x82067c20`;
- `DumpRange.java 0x8228e900..0x8228ec80`;
- `ReferencesTo.java 0x821d1128`;
- `FindDirectCallsTo.java` et `FindPpcRawBranchesTo.java` pour
  `0x821d1128`, `0x821d1060`, `0x820a85e0` et `0x8228e988`;
- `TraceGlobalVirtualSlot.java 0x8293ba10 0x8 0x20`;
- toutes les commandes ont utilisé le projet canonique en lecture seule.
