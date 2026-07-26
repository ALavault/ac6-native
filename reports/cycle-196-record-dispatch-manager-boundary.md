# AC6 cycle 196 — frontière du dispatcher de records et du manager runtime

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`

Passe statique, headless et en lecture seule. Aucun projet Ghidra, binaire,
sortie générée ou configuration runtime n'a été modifié.

## Dispatcher `0x8212b8ac`

Le dump PPC borné `0x8212b700..0x8212c018` réconcilie les appels observés au
cycle 195 :

- le cas `0x3001` résout un record, extrait un quadruplet via `0x821265e8`,
  l'écrit dans `0x823fb360+0x04..+0x10` si le byte
  `0x829188a0+0x16a6` est actif, puis pose `0x829188a0+0x24 = 1` ;
- les cas `0x3002` et `0x3003` utilisent `0x82126a60` et `0x82126938`, puis
  publient leurs résultats dans les buffers globaux `0x829bafc8..` et
  `0x829bafd0..` (`+0x137c`/`+0x1380` depuis la base matérialisée) ;
- les cas `0x3005..0x3012` passent par une table de dispatch vers plusieurs
  extracteurs voisins, dont `0x82126b90` ; ils lisent des records typés et
  publient des scalaires ou pointeurs, pas une matrice de pose identifiée ;
- les cas `0x3033` et `0x3034` résolvent un record, appellent `0x82127440`, puis
  invoquent un slot `+0xb0` d'un objet de service avec `record+0x20` et un
  indicateur dérivé du code ;
- le cas `0x3035` appelle les slots `+0x48` et `+0x4c` du même objet avec les
  flottants `record+0x18` puis `record+0x14` ;
- le cas `0x8001` active un état manager (`0x829188a0+0x16a7`) ;
- le cas `0x8002` ajoute au plus quatre entrées dans la zone pointée par
  `manager+0x20`, incrémente le compteur et recopie une valeur sélectionnée
  dans le record à `+0x14`.

Ces branches ferment la frontière entre les records de ressources/mouvement et
un manager runtime. Les slots virtuels sont bien consommés avec des scalaires
et des pointeurs de records, mais leur cible effective reste runtime-initialisée
et leur rôle métier n'est pas prouvé.

## Initialisation et état du contexte

`0x82130948` initialise les globals associés :

- `0x823fb394` et `0x823fb370` reçoivent la constante zéro/epsilon globale ;
- `0x823fb39c` reçoit `-1` ;
- `0x823fb3a0` reçoit zéro ;
- plusieurs compteurs voisins `0x829bafc8..0x829bafe8` sont remis à zéro ;
- l'appel `FUN_82332470(0x8002, 0x20)` configure explicitement le chemin de
  dispatch/stockage correspondant au cas `0x8002`.

Les méthodes `0x82267da8`, `0x82267ee8`, `0x822685e0` et `0x822686e0`
confirment un cycle de flags et de transitions :

- `0x82267da8` lit le bit 1 de `0x823fb3a0` ;
- `0x82267ee8` et `0x822686e0` réinitialisent `0x823fb394`, `0x823fb370`,
  `0x823fb39c`, `0x823fb3a0` et des champs voisins ;
- `0x822685e0` pose le bit `0x4` de `0x823fb3a0` dans un chemin d'activation.

Cela correspond à un état de transition/service partagé, et non à une preuve
que les quatre composantes de `0x823fb360` sont une position, un quaternion ou
une vitesse d'avion.

## Limite sémantique

La meilleure qualification actuelle est :

- `confirmed` : codes du dispatcher, offsets, gardes, écritures de buffers,
  compteur/table `manager+0x20`, appels slots `+0x48/+0x4c/+0xb0` ;
- `cross-match` : pipeline de paramètres/records de mouvement et contexte
  d'interpolation ;
- `unknown` / `needs-dynamic-evidence` : classe concrète du manager, identité
  des slots, relation à l'avion joueur, caméra, trajectoire ou contrôleur de
  vol.

La chaîne statique ne justifie pas encore un helper natif nommé
`PlayerAircraft`, `CameraController`, `SetPosition` ou `FlightUpdate`.
Attribuer l'un de ces noms serait une extrapolation non prouvée.

Une trace runtime qualifiée pourra un jour corréler les slots et les valeurs
avec l'avion actif, mais aucune intervention humaine n'est requise pour cette
tranche statique. Le prochain gain statique éventuel est de résoudre le vtable
runtime du manager ; si cela reste impossible depuis l'image, la frontière
sera conservée comme `needs-dynamic-evidence` plutôt que régénérée sans
information nouvelle.

## Validation

- Ghidra 12.1.2 headless, `DumpRange.java`, `DecompileAt.java` et
  `FindPpcAddressMaterialization.java`, en lecture seule.
- Le projet a été temporairement verrouillé par une série de diagnostics
  parallèles ; ces processus ont été arrêtés proprement avant la passe finale.
- Aucun writer Ghidra, Xenia, Wine, GUI, VNC ou run humain n'a été utilisé.
- Les validations natives AC6 restent celles du dernier gate : CTest **41/41**
  et launcher Xenia/Wine `status=ready`, `release=16e1eb8`, `renderer=vulkan`.
