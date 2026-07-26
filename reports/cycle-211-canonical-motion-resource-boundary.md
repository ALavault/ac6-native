# AC6 cycle 211 — canonical motion/resource boundary

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet : `workspaces/ace-combat-6/ghidra-projects/ace-combat-6`
- mode : Ghidra headless, `-readOnly -noanalysis`.

Cette passe sépare deux familles qui utilisent des offsets numériques proches :
le gestionnaire de requêtes de mouvement et les consommateurs de ressources
attachées à `objet+0x15c`. Les offsets seuls ne permettent pas de fusionner ces
objets avec le `CX360UnitManager` ou avec un objet de vol.

## Gestionnaire de requêtes de mouvement

Le vtable à `0x8205cd90` est accompagné de la chaîne de type
`CX360MotionRequestManager` autour de `0x8205cddc`. Le global
`0x82697c00` pointe vers cette table et `0x82697c04` pointe vers le global.
Le constructeur `0x8211bc88` installe la table. Les deux slots examinés sont :

- `+0x04 -> 0x8211bcd0`, qui transmet à `0x82118a50` ;
- `+0x08 -> 0x8211bcd8`, qui inspecte le champ de type à `record+0x0a` et
  branche vers `0x82339798` pour le tag `0x11`, ou vers `0x8211bb80` pour le
  tag `0x8181` (`-0x7e7f` signé dans la décompilation).

Les fonctions `0x82136100` et `0x821371d8` appellent ensuite le slot `+0x08`
avec respectivement `objet+0x1a4` et `objet+0x1a8` (les deux dans le premier
cas). Les seuls sites directs observés pour ce slot sont `0x82136134`,
`0x8213614c` et `0x82137208`; ils ne proviennent pas du chemin
`CX360UnitManager -> 0x820a8678` revalidé au cycle 210.

Un bloc PPC brut autour de `0x82127eb0..0x82127f30` écrit les champs
`+0x18c`, `+0x190`, `+0x194`, `+0x1a0`, `+0x1a4` et `+0x1a8`, puis appelle
`0x82136168`. Le site direct canonique trouvé est `0x82127f30`. La frontière
de fonction et le type concret du contexte ne sont pas récupérés par Ghidra;
ces champs restent donc `offset/role` et non des champs gameplay nommés.

Qualification : `confirmed` pour la table, les slots, les tags et les
transmissions d'offsets ; `unknown`/`needs-types` pour le type du record et la
signification gameplay de ces requêtes. Aucun appel n'établit une mission, un
appareil actif ou une transition post-CUT.

## Consommateurs de la ressource `+0x15c`

Les consommateurs canoniques examinés sont les suivants :

- `0x82226c20` teste `param+0x15c`, lit un état via `0x8225c0b8`, puis appelle
  `0x82223ac0(resource,1)` ou `0x82223ac0(resource,2)` et renvoie la ressource.
  C'est un cycle de vie de ressource, pas un identifiant d'aéronef.
- `0x8228e9e8` résout `+0x15c` par `0x8212f2c0`, parcourt la chaîne de
  records via `0x821d10d0`, mémorise un record de type `0x40` à `+0x328` et
  positionne le bit `0x40` de `+0x2f8` lorsqu'un record `0x12` est présent.
- `0x8228fc80` utilise ce bit pour contrôler des bornes flottantes à
  `+0x100/+0x104/+0x108`, puis appelle `0x82223538` avec la ressource et le
  tag `0x12`. Ghidra signale une instruction problématique dans cette zone;
  le rôle exact reste donc `dynamic`/`unknown`.
- `0x82293d08` demande à `0x82223338` les propriétés `0x34c`, `0x353` et
  `0x355` de la ressource si `+0x15c` est non nul.
- `0x82374590` traite `+0x15c` comme une base de tableau pour une agrégation;
  `0x82374978` libère `+0x140` et `+0x15c`. Cette autre famille est un cycle
  de données/tableau et ne doit pas être fusionnée avec le manager.

À l'inverse, `0x821d8e00` écrit `+0x15c` dans un constructeur distinct. Les
écritures homonymes ne fournissent aucune provenance vers l'objet manager.

## Conclusion et frontière

La passe confirme un producteur de requêtes de mouvement et plusieurs
consommateurs de ressources, mais aucun joint statique vers :

- le receiver partagé `DAT_8293BA10 + 8` ;
- le mapping campagne du selector `1` ;
- un consommateur métier de `CutTerminate` ;
- un objet `MissionAircraft`, un spawn ou un gameplay-camera owner.

AC6 reste donc `native-partial`, avec la frontière native `scene_complete`.
Cette limite est une absence de preuve statique, pas un blocage demandant une
action humaine. Les sessions runtime peuvent rester différées jusqu'à ce
qu'un des trois joints ci-dessus soit récupéré ou qu'une expérience ciblée
devienne nécessaire.

## Validation

- `DecompileMany.java` sur les slots du motion manager, les consommateurs
  `+0x15c` et les helpers de ressources ;
- `DumpRange.java` sur `0x82127b80..0x82128120` pour le producteur brut ;
- `InspectFunctionIsland.java` sur `0x82127000..0x82128500` ;
- `FindDirectCallsTo.java` sur les slots et helpers ;
- toutes les opérations sur le projet canonique PAL, en lecture seule ;
- CTest AC6 : **41/41 PASS** (run portefeuille courant) ;
- oracle Xenia/Wine : `status=ready`, release `16e1eb8`, renderer `vulkan`,
  service `ac6-xenia-wine-gui.service`.

