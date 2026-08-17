# Cycle 1698 — A/B des loads guest et sites générés au tick 268

## Résultat

La sonde read-only `AC6_DEMO_WATCH_TICK_LOADS=1`, limitée à
`[0x2E3D3C00,0x2E3D4500)` et aux ticks 268–269, capture les consumers
potentiels qui suivent le différentiel du cycle 1697. Le run START contient
148 loads et neutral 147; 141 lignes sont strictement communes.

Les sept loads START-only sont tous sur le thread 1. Ils relisent le vtable
`0x2E3D3D14 -> 0x820077AC`, puis des champs liés au thunk
`0x820E1F78`/`0x820E7E08` (`0x2E3D3E68`, `0x2E3D4050`, `0x2E3D3D18`,
`0x2E3D3D20`). Les six loads neutral-only concernent une autre structure
`0x2E3D44D0..0x2E3D44F0`, avec les valeurs `0`, `0xFFFFFFFF` et
`0x2DF1121C`. Cela joint les sites de lecture aux bytes générés, mais ne
prouve pas encore qu’un load consomme la valeur écrite ni qu’il commande un
frontend.

## Identité et protocole

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| basefile | SHA-256 `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA-256 `72701e4729db70605ad1f8303d60267f4b7e67c2384862cf12dc1d8f696708d7` |
| source principal | `src/guest_bridge.cpp`, SHA-256 `e150cdfb1357659b30b460c1cb0ffba15cd6700373642d9c9d8172f262f20e22` |
| fragment de sonde | `src/guest_bridge/transition_memory_trace.hpp`, SHA-256 `d3ed18ebbb97e685b452fe89d07d3851e29f29e4f2171d8d34720af87bf6088f` |
| filtre | `AC6_DEMO_WATCH_TICK_LOADS=1`, `AC6_DEMO_WATCH_EVENT_HANDLE_CONSUMERS=1`, plage `0x2e3d3c00:0x2e3d4500`, ticks 268–269 |
| entrée START | bouton `0x10` tenu aux ticks 252–267 |

Le hook de site généré ne sert qu’à imprimer le symbole/ligne déjà fournis
par le codegen; il ne change ni la valeur chargée ni l’ordonnancement. Chaque
route a un processus et un store neufs.

## Reçus A/B

| route | `AC6_TICK_LOAD` | lignes communes | stderr SHA-256 | RTPLY SHA-256 | rapport SHA-256 | résultat |
|---|---:|---:|---|---|---|---|
| START | 148 | 141 | `2b4d71874e342162624327ccccd69d4c5dbf325a756b24324372776fcf28fcbf` | `4bf229f4a691d28efb6e979b717120955f7337388190e7109f05bcfa9fb1c273` | `598c6f84a5fbfc44aaeca2c9fb4cacafa1f38c2d2fc81d823971a9c8cb20c621` | 269 ticks, 132 PRESENT |
| neutral | 147 | 141 | `72eeee5e6d1c62b4fb1843c00bb7015a320b1d4f04ee23a7285e4021b4774adc` | `919dd6f1fc894526a444d12f0968153cad8acc1cd6baa37ad022a6255a72df7e` | `aacdebaf4412ad38104ccda80eba0163e7e801272d50433af494af5a6b6d9d72` | 269 ticks, 132 PRESENT |

Les deux rapports restent `frontend=false`, `mission=false`, `terminal=false`
et `outcome=max_ticks`. Les RTPLY/rapports sont identiques à ceux du cycle
1697 car la sonde est purement hors chemin de résultat.

## Différentiel exact des loads

### START-only

| adresse | valeur | LR | fonction/ligne générée |
|---|---|---|---|
| `0x2E3D3C0C` | `0x00000000` | `0x820CDC20` | `sub_82321E20`, 24471 |
| `0x2E3D3D14` | `0x820077AC` | `0x82321F34` | `sub_823241D0`, 5188 |
| `0x2E3D3E68` | `0x2E3D4050` | `0x820E1F80` | `sub_820E1F78`, 31532 |
| `0x2E3D4050` | `0x2E3D4050` | `0x820E1F80` | `sub_820E1F78`, 31536 |
| `0x2E3D3D18` | `0x2E3D3AD4` | `0x820E1FC4` | `sub_820DD528`, 20549 |
| `0x2E3D3D20` | `0x2E3CF150` | `0x820D9508` | `sub_820D9500`, 11007 |
| `0x2E3D3D20` | `0x2E3CF150` | `0x820DC680` | `sub_820DC660`, 18316 |

### Neutral-only

| adresse | valeur | LR | fonction/ligne générée |
|---|---|---|---|
| `0x2E3D44E8` | `0x00000000` | `0x820CDF24` | `sub_82321E20`, 24538 |
| `0x2E3D44EC` | `0x00000000` | `0x823255F0` | `sub_823255E8`, 8350 |
| `0x2E3D44F0` | `0xFFFFFFFF` | `0x823255F0` | `sub_823255E8`, 8344 |
| `0x2E3D44F0` | `0x00000000` | `0x823255F0` | `sub_823255E8`, 8355 |
| `0x2E3D44D0` | `0x2E3D3AD4` | `0x823255F0` | `sub_823255E8`, 8363 |
| `0x2E3D44DC` | `0x2DF1121C` | `0x823255F0` | `sub_823255E8`, 8367 |

## Classification et garde

- `demo-qualified` : identité PAL, A/B frais, plage/tick exacts, 141 loads
  communs et tous les PC/LR/valeurs du différentiel ci-dessus.
- `demo-observed` : relecture START du vtable et des champs du thunk au tick
  268; association aux lignes générées.
- `xenia-generic` : aucun élément.
- `unknown` : ordre load→store, consumer métier, état frontend, pixels,
  audio, mission et terminal.

La garde exige une nouvelle sonde ordonnée (stores et loads dans le même
passage) ou un watchpoint rr avant de qualifier une causalité. Aucun nom de
classe déduit des symboles générés, aucun fallback renderer et aucune preuve
retail ne sont introduits.

