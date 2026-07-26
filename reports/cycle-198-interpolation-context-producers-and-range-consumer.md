# AC6 cycle 198 — producteurs du contexte d'interpolation et consommateur de bornes

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`

Passe statique headless, en lecture seule, sur le projet Ghidra canonique.
Les artefacts proviennent de `FindPpcAddressMaterialization.java`,
`FindDirectCallsTo.java`, `DecompileAt.java` et `DumpRange.java`. Aucun projet
Ghidra, binaire, fichier généré ou configuration runtime n'a été modifié.

## Contrat du contexte `0x822aaeb8`

Le helper reçoit un contexte à `param_2` et remplit quatre familles de champs :

- `+0x04..+0x10` : quatre valeurs de départ, copiées depuis `param_6` si ce
  pointeur est non nul ;
- `+0x14..+0x20` : quatre valeurs d'arrivée, copiées depuis `param_5` ;
- `+0x24..+0x30` : différences arrivée-départ, multipliées par
  `DAT_82001348 / context+0x34` ;
- `+0x34` : durée ou facteur fourni par `param_1` ;
- `+0x38` : constante `DAT_8200082c` ;
- `+0x3c` : type/statut fourni par `param_4` ;
- `+0x40` : état posé à `3`.

Le layout et l'ordre des opérations sont `confirmed`. L'interprétation métier
reste un contexte de transition/interpolation de quatre composantes, pas une
pose d'avion démontrée.

## Producteurs directs

Les appels directs à `0x822aaeb8` confirment plusieurs familles de
producteurs :

- `0x82250e18` refuse un objet dont `+0x68` porte le bit `0x40000000`, remet
  à zéro `0x823fb394`, `0x823fb370`, `0x823fb39c` et `0x823fb3a0`, puis appelle
  le helper avec une durée `param_1 + DAT_82007e68`, le quadruplet d'arrivée
  `0x82765b78 = (0,0,0,0)` et le quadruplet de départ
  `0x823fb340 = (0,0,0,1)` ;
- `0x822515d0`, pour ses états `1`/`4` et lorsque le même bit n'est pas actif,
  effectue le même reset et utilise la durée `DAT_82005eec`; son état `3`
  suit un chemin audio/service distinct et ne doit pas être fusionné avec
  l'interpolation ;
- `0x82267dc0` appelle d'abord `0x82254820`, puis initialise le contexte avec
  ses paramètres et les mêmes quadruplets par défaut ;
- `0x82267e28` réalise une initialisation paresseuse protégée par le bit
  `param+4 & 8`, met à jour `param+0x174`, `param+0x178` et `param+0x17c`,
  puis appelle le helper avec `param_6 = nullptr` ;
- les branches brutes autour de `0x82255ee4` et `0x82255f58` appellent aussi
  le helper avec des durées/valeurs constantes propres à leur état local ;
- `0x8226a088` appelle le helper sur `0x823fb360` avec une constante de durée,
  `0x82765b78` et `0x823fb340`, puis poursuit vers un consommateur de bornes
  local.

Ces producteurs établissent une politique de reset et d'interpolation
partagée. Ils ne fournissent toujours pas un nom stable d'avion, de caméra ou
de trajectoire.

## Consommateur de bornes `0x82268b28`

Après l'appel de `0x822aaeb8` dans la branche `0x8226a088`, le code sélectionne
un objet auxiliaire à `param+0x274` ou `param+0x270`, sauf lorsque son byte
`+0xa6` vaut `2`. Il lit alors quatre flottants aux offsets `+0x28`, `+0x30`,
`+0x34` et `+0x3c`, puis appelle `0x82268b28`.

La décompilation de `0x82268b28` est sans ambiguïté : elle compare les deux
paires `(f1,f3)` et `(f2,f4)` et écrit leurs minimums/maximums dans l'objet
sortie aux offsets `+0x28c`, `+0x294`, `+0x290` et `+0x298`. Il s'agit d'un
calcul de bornes/range sur quatre scalaires. Les valeurs peuvent être liées à
un système de scène ou de mouvement, mais rien ne permet de les nommer
position, altitude, vitesse ou limites de vol.

## Réinitialisation et flags de cycle

- `0x82267ee8` efface des flags de l'objet appelant, remet à zéro
  `0x823fb394`, `0x823fb370`, `0x823fb39c` et `0x823fb3a0`, en conservant
  conditionnellement un champ de durée local ;
- `0x822685e0` pose le bit `0x4` de `0x823fb3a0` après les gardes d'état et
  les callbacks de service ;
- `0x822686e0` efface le bit `0x100000` d'un objet, effectue la transition
  d'état, puis efface le bit `0x4` de `0x823fb3a0` et réinitialise les champs
  adjacents `0x823fb3d8`, `0x823fb3b4`, `0x823fb3e0` et `0x823fb3e4` ;
- le fragment `0x82268778` reproduit la fermeture du bit `0x4` et le reset de
  ces champs dans une autre branche de la même machine d'état.

Les flags sont donc des états de cycle distincts des quatre composantes
flottantes. Ne pas les transformer en `aircraft_active` ou `flight_state` sans
trace dynamique.

## Qualification et blocage humain

- `confirmed` : layout du contexte, calcul des différences, durée/facteur,
  statut/état, producteurs directs, resets et calcul min/max de
  `0x82268b28` ;
- `cross-match` : pipeline de transition/animation ou de paramètres de scène ;
- `unknown` / `needs-dynamic-evidence` : relation à l'avion actif, à la
  caméra, au vol ou à une trajectoire observable.

Aucune intervention humaine n'est requise pour cette tranche. Une session
Xenia ne deviendra nécessaire que pour attribuer une sémantique gameplay aux
quadruplets et aux bornes ; elle n'est pas un prérequis à la poursuite de la
transcription statique.

## Validation

- `FindPpcAddressMaterialization.java` sur `0x823fb360` et les globals voisins ;
- `FindDirectCallsTo.java` sur `0x822aaeb8`, `0x82267ee8` et
  `0x822686e0` ;
- `DecompileAt.java` sur `0x822aaeb8`, `0x82250e70`, `0x82251658`,
  `0x82267e10`, `0x82267ecc`, `0x82267da8`, `0x822686e0`,
  `0x82267ee8` et `0x82268b28` ;
- `DumpRange.java` sur les branches `0x82255d80..0x82256020`,
  `0x82268680..0x82268840` et `0x82269e80..0x8226a180` ;
- CTest AC6 : dernier gate connu `41/41` ;
- launcher Xenia/Wine : dernier état connu `status=ready`, `release=16e1eb8`,
  `renderer=vulkan`.

