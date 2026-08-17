# Cycle 1615 — jointure Xenia générique du resolve neutral

## Autorité générique

Checkout Xenia local read-only :
`tools/xenia-source`, commit
`95a5c3ee250f80c3b9d139658649d9ffb6db3eec`, worktree propre. Les symboles
consultés via `git show/grep`, sans élargir ni modifier le sparse checkout :

- `CommandProcessor::ExecutePacketType0`, `WriteRegister`,
  `ExecutePacketType3_XE_SWAP` dans `src/xenia/gpu/command_processor.cc` ;
- `RegisterFile::GetVertexFetch/GetTextureFetch` dans `register_file.h` ;
- `reg::RB_SURFACE_INFO`, `RB_COLOR_INFO`, `RB_COPY_CONTROL`,
  `RB_COPY_DEST_PITCH`, `RB_COPY_DEST_INFO` dans `registers.h` ;
- `draw_util::GetResolveInfo` dans `draw_util.cc` ;
- `xenos::Endian128`, `EdramMode`, `CopyCommand`, les structures fetch et les
  règles d'alignement resolve dans `xenos.h` ;
- les chemins Vulkan `VulkanCommandProcessor` et
  `VulkanRenderTargetCache::Resolve` comme référence d'implémentation future.

Ces sources sont `xenia-generic`, jamais une preuve AC6.

## Gate PM4

`demo-observed` : le packet `e4356ed3…7312` écrit les valeurs exactes
`C0100000 07F00000 C0000000 00100000` dans `0x0A02..0x0A05`. Les autres
registres anonymes atteints (`0x2290/91`, `0x230B..11`, `0x2313/14`) ne reçoivent
que zéro.

`demo-qualified` : la petite table locale admet uniquement ces couples
indice/valeur comme stockage opaque, conformément au chemin type-0 générique
Xenia. Une valeur divergente trappe avant commit. L'inventaire
`analysis/demo/ac6-demo-neutral-after-230b-pm4-inventory-v1.json`, SHA-256
`cd3a37a2…5e868`, consomme exactement 3029 dwords et 871 packets, sans
resynchronisation, et ne contient plus d'opcode, registre ou longueur inconnu.

## Resolve neutral joint au swap

Les valeurs ci-dessous proviennent directement de l'état register file au
second `DRAW_INDX_2`, offset dword 387 du main IB. Leur décodage de champs vient
de Xenia :

| Niveau | Champ | Valeur |
|---|---|---|
| demo-observed | `RB_MODECONTROL` | `0x00000006` |
| xenia-generic | mode 6 | `EdramMode::kCopy` |
| demo-observed | `RB_COPY_CONTROL` | `0x00100000` |
| xenia-generic | commande bits 20..21 | `CopyCommand::kConvert`, source color 0 |
| demo-observed | RT source | `RB_SURFACE_INFO=0x14000500`, `RB_COLOR_INFO=0` |
| demo-qualified | source | EDRAM color 0, base tile 0, pitch 1280, MSAA brut 0, format RT brut 0 |
| demo-observed | scissor | TL `0`, BR `0x02D00500` |
| demo-qualified | scissor | 1280×720 |
| demo-observed | destination | base `0x1374A000`, pitch `0x02D00500`, info `0x01000300` |
| demo-qualified | destination | 1280×720, format 6, endian 0, tiled, swap bit 1 |
| demo-observed | vertex fetch 0 | `127CA093 1000001A` |
| demo-qualified | fetch resolve | type vertex, adresse `0x127CA090`, 6 dwords, endian mode 2 |
| demo-observed | vertex bytes | SHA-256 `1187ed99…0fa12` |
| demo-qualified | vertices après endian | `(-0.5,-0.5)`, `(1279.5,-0.5)`, `(1279.5,719.5)` |
| demo-observed | `XE_SWAP` | offset 415, `0x1374A000`, 1280×720 |

Le draw copy précède donc causalement le fetch final puis `XE_SWAP`. L'observer
opt-in `AC6_DEMO_WATCH_RESOLVE` capture les 24 bytes au moment du draw, sans les
publier dans un artefact suivi. Avec le demi-pixel Xenia, le scissor et
l'alignement 8×8, le rectangle resolve est exactement `(0,0)-(1280,720)`.
Le frontbuffer, son adresse, pitch, format, endian, tiling et rectangle resolve
sont donc qualifiés. Le prochain champ `unknown` est le contenu EDRAM color 0 :
le processeur courant inventorie les draws mais ne les rasterise pas encore.
Aucun readback ne peut être présenté comme image guest avant exécution exacte
des deux shaders/draws atteints et du resolve tiled.

## Validation

- inventaire réel : 3029/3029 dwords, 871 packets, `first_unknown=null` ;
- tests de divergence des registres opaques : PASS transactionnel ;
- test durable des packets resolve offsets 326/387/415 : PASS ;
- Xenia, Ghidra, C++ généré et microcodes : inchangés ;
- aucun actif propriétaire suivi.

Prochain test minimal : joindre les snapshots de registres des deux draws à
leurs fetch/constants et exécuter le rectangle normal dans un EDRAM logiciel
borné, puis appliquer le resolve exact ci-dessus et comparer deux readbacks
neutral frais.
