# AC6 cycle 193 — consommateur `CX360MotionRequestManager` des records

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`
- projet Ghidra : `ace-combat-6`

Analyse headless, statique et en lecture seule. Aucun projet Ghidra, XEX,
runtime Xenia/Wine ou sortie générée n’a été modifié.

## Consommateur aval dans `0x821c1560`

Le chemin appelé lorsque `r4` est fourni à `0x821c14a0` construit une vue
secondaire et parcourt ses entrées. Pour chaque entrée, il résout un record avec
`0x82234dd0`, puis appelle indirectement un objet global :

```text
0x821c1670  r29 = *(0x82697c04) = 0x82697c00
0x821c167c  r28 = *(r29)       = 0x8205cd90   (vtable)
0x821c1680  r4  = record résolu dans la vue secondaire
0x821c1688  r11 = *(vtable + 0x08)
0x821c1694  bctrl
```

La zone de vtable `0x8205cd90` contient la chaîne ASCII
`CX360MotionRequestManager` à partir de `0x8205cddc`. Cela établit une
association statique avec un gestionnaire de requêtes de mouvement, au niveau
du libellé RTTI/objet et du slot virtuel appelé. Le contrat exact du service
reste volontairement nommé `motion_request_manager` : le libellé ne prouve pas
à lui seul s’il traite une animation, une trajectoire ou un autre payload de
mouvement.

## Contrat du slot `0x8211bcd8`

Le slot `vtable+0x08` (`0x8211bcd8`) lit le type 16 bits big-endian du record à
`record+0x0a` :

```text
type 0x0011 -> 0x82339798
type 0x8181 -> 0x8211bb80
autre       -> retour sans traitement
```

### Type `0x0011`

`0x82339798` vérifie le flag de représentation au bit `0x80000000`, appelle
`0x82345728` si le type est `0x0011`, puis efface ce flag. `0x82345728` parcourt
les éléments, efface leur champ `+0x0c` et convertit leurs pointeurs absolus en
offsets relatifs par rapport au record. C’est une normalisation de payload.

### Type `0x8181`

`0x8211bb80` vérifie le même flag, parcourt `record+0x14` éléments, puis pour
les sous-records de type `1` :

- efface le champ `subrecord+0x34` ;
- convertit les tableaux de sous-éléments de pointeurs absolus en offsets
  relatifs ;
- convertit la liste des éléments et le pointeur final en offsets relatifs ;
- efface le bit `0x80000000` du record principal.

Ce chemin est compatible avec une désérialisation/nettoyage de records de
mouvement ou de ressources, mais ne contient pas de store direct vers les
champs de pose `+0x50/+0x54/+0x58`, la caméra, les contrôleurs de vol ou le
writer global `0x8226f050`.

## Conséquence pour `MissionAircraft`

La chaîne statique peut désormais être écrite :

```text
ResourceManager::MissionAircraft
  -> nœuds hiérarchiques et vues node+0x18
  -> records indexés 0..4
  -> traitement de payload dans ResourceManager+0x14
  -> CX360MotionRequestManager::slot+0x08
  -> normalisation de records de types 0x0011/0x8181
```

Cette chaîne est une piste plus forte vers des données de mouvement, mais elle
ne démontre toujours pas que `MissionAircraft` désigne l’avion joueur, ni que
le slot écrit un état de vol ou une caméra. Les qualifications restent :

- `confirmed` : objet/vtable `CX360MotionRequestManager`, slot appelé, types
  `0x0011` et `0x8181`, normalisation des représentations ;
- `cross-match` : hypothèse de payload de mouvement/trajectoire ;
- `unknown` / `needs-dynamic-evidence` : propriétaire gameplay, consommateur
  final, lien avec l’avion actif et valeurs runtime.

## Suite et action humaine

La prochaine piste statique est de recenser les autres appels au slot
`0x8211bcd8` et les producteurs des records de types `0x0011`/`0x8181`, puis de
comparer leurs champs et leurs appelants. Aucun run humain, VNC, GUI ou
interaction clavier n’est nécessaire pour continuer.

