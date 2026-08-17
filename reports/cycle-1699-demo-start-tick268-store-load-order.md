# Cycle 1699 — ordre store/load et garde de causalité START

## Résultat

Une capture combinée read-only (`AC6_DEMO_WATCH_TICK_STORES=1` et
`AC6_DEMO_WATCH_TICK_LOADS=1`) rejoue neutral et START dans le même processus
de capture, bornée à `[0x2E3D3C00,0x2E3D4500)` au tick 268. Elle permet de
qualifier l’ordre des effets sans déduire une sémantique de classe.

Sur START, l’unique écriture spécifique `0x2E3D3C0C <- 0` est précédée par
un load de cette même adresse et n’est suivie d’aucun load dans la fenêtre.
Le vtable `0x2E3D3D14` et les champs du thunk sont ensuite relus. Cette
séquence ne prouve donc pas un consumer de l’état écrit, et START ne peut pas
encore être promu comme transition visuelle.

Sur neutral, la structure `0x2E3D44F0` suit l’ordre
`load(FFFFFFFF) → load(0) → store(0) → load(0) → store(FFFFFFFF)`, avec les
loads auxiliaires `0x2E3D44D0/44DC`. C’est un chemin distinct, non une preuve
retail ou Xenia-générique.

## Identité et protocole

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| basefile | SHA-256 `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA-256 `72701e4729db70605ad1f8303d60267f4b7e67c2384862cf12dc1d8f696708d7` |
| fragment sonde | `src/guest_bridge/transition_memory_trace.hpp`, SHA-256 `d3ed18ebbb97e685b452fe89d07d3851e29f29e4f2171d8d34720af87bf6088f` |
| filtre | plage `0x2e3d3c00:0x2e3d4500`, ticks 268–269, hooks store/load + sites générés |
| entrée START | bouton `0x10` tenu aux ticks 252–267 |

Chaque route utilise un store et un processus neufs. Les hooks sont désactivés
par défaut et ne modifient ni mémoire ni ordre guest.

## Reçus A/B

| route | stores | loads | événements ciblés | stderr SHA-256 | RTPLY SHA-256 | rapport SHA-256 |
|---|---:|---:|---:|---|---|---|
| START | 67 | 148 | 215 | `69a63832eb0c29b6a88156ca1298dee180adea1a2ec4b47c6a67cc0f4736000f` | `4bf229f4a691d28efb6e979b717120955f7337388190e7109f05bcfa9fb1c273` | `598c6f84a5fbfc44aaeca2c9fb4cacafa1f38c2d2fc81d823971a9c8cb20c621` |
| neutral | 68 | 147 | 215 | `83f74b7b7305f81ee87b671ac1c59fa91704278e2a563574209c900bc4b62b0e` | `919dd6f1fc894526a444d12f0968153cad8acc1cd6baa37ad022a6255a72df7e` | `aacdebaf4412ad38104ccda80eba0163e7e801272d50433af494af5a6b6d9d72` |

Les deux rapports atteignent 269 ticks/132 PRESENT, avec
`frontend=false`, `mission=false`, `terminal=false` et `outcome=max_ticks`.

## Ordre exact START

Les numéros de ligne sont ceux du stderr brut START.

| ordre | opération | adresse | valeur | PC/LR | fonction/ligne |
|---:|---|---|---|---|---|
| 1 | load | `0x2E3D3C0C` | `0x00000000` | `0x820CDC20` | `sub_82321E20:24471` |
| 2 | store | `0x2E3D3C0C` | `0x00000000` | `0x820CDC20` | `sub_82321E20:24475` |
| 3 | load | `0x2E3D3D14` | `0x820077AC` | `0x82321F34` | `sub_823241D0:5188` |
| 4 | load | `0x2E3D3E68` | `0x2E3D4050` | `0x820E1F80` | `sub_820E1F78:31532` |
| 5 | load | `0x2E3D4050` | `0x2E3D4050` | `0x820E1F80` | `sub_820E1F78:31536` |
| 6 | load | `0x2E3D3D18` | `0x2E3D3AD4` | `0x820E1FC4` | `sub_820DD528:20549` |
| 7 | load | `0x2E3D3D20` | `0x2E3CF150` | `0x820D9508` | `sub_820D9500:11007` |
| 8 | load | `0x2E3D3D20` | `0x2E3CF150` | `0x820DC680` | `sub_820DC660:18316` |

Il n’y a aucun load START de `0x2E3D3C0C` après l’ordre 2 dans cette fenêtre.

## Ordre exact neutral

| ordre relatif | opération | adresse | valeur | PC/LR | fonction/ligne |
|---:|---|---|---|---|---|
| 1 | load | `0x2E3D44F0` | `0xFFFFFFFF` | `0x823255F0` | `sub_823255E8:8344` |
| 2 | load | `0x2E3D44EC` | `0x00000000` | `0x823255F0` | `sub_823255E8:8350` |
| 3 | store | `0x2E3D44F0` | `0x00000000` | `0x823255F0` | `sub_823255E8:8352` |
| 4 | load | `0x2E3D44F0` | `0x00000000` | `0x823255F0` | `sub_823255E8:8355` |
| 5 | load | `0x2E3D44D0` | `0x2E3D3AD4` | `0x823255F0` | `sub_823255E8:8363` |
| 6 | load | `0x2E3D44DC` | `0x2DF1121C` | `0x823255F0` | `sub_823255E8:8367` |
| 7 | store | `0x2E3D44F0` | `0xFFFFFFFF` | `0x82325644` | `sub_823255E8:8404` |

## Classification et garde

- `demo-qualified` : identité PAL, A/B frais, ordre interleavé exact, PC/LR,
  lignes générées et valeurs bornées.
- `demo-observed` : START-only load/store et chemin neutral distinct.
- `xenia-generic` : aucun élément.
- `unknown` : consumer métier après le thunk, état visuel, pixels, audio,
  mission et terminal.

La garde de causalité reste fermée : aucun état frontend n’est écrit depuis le
runtime et aucun readback n’est promu sur cette seule séquence. Le prochain
test doit joindre ces opérations à un watchpoint rr ou à un consumer guest
ultérieur, puis vérifier une divergence de readback; toute identité inconnue
doit trap avant effet.
