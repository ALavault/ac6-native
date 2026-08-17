# Cycle 1663 — garde de provenance du draw normal PAL

## Résultat

Le chemin Vulkan normal ne consomme désormais qu’un `XenosDrawCommand` PAL
exact : hashes vertex/pixel, rectangle-list, auto-index, 3 indices, format
16-bit, prédication active et profil complet des registres
`RB_SURFACE_INFO=0x0A020280`, `RB_COLOR_MASK=0x0000FFFF`,
`RB_BLENDCONTROL0=0x10010001`, `RB_DEPTHCONTROL=0x00008777`,
`RB_DEPTHCONTROL1=0x00010001`, `RB_MODECONTROL=0x4`. Toute divergence trappe
avant le draw.

Les snapshots sont conservés comme valeurs immuables entre batches PM4 : le
draw normal et le copy arrivent séparément, ce qui est maintenant couvert par
les optionnels de la frontière renderer plutôt que par des pointeurs de batch.

Deux exécutions codegen ON/Vulkan PAL ont été rejouées après cette garde :
neutral et START (`0x0010` au tick 252), jusqu’au tick 253. Elles donnent les
mêmes rapports graphics, 116 PRESENT, les IB `ef7ab6e4…d2b0` et
`d121c8d8…358d6`, le readback normal noir `0b150fd3…ec58366` et le resolve
`0c660f2b…a4913a5f`. START reste non promu.

## Limite

Cette garde joint maintenant le draw Vulkan aux bytes PM4 exacts, mais ne
produit toujours pas une destination guest-owned : le contenu EDRAM RT0 et la
projection neutre restent limités au corridor qualifié. Aucune screencap n’est
donc produite.

## Vérifications

- CTest démo OFF : `18/18`.
- CTest codegen ON : `17/17`.
- `spirv-val` du resolve ReXGlue : PASS.
- Traces/rapports A/B : hashes conservés dans la capsule.

Capsule durable : `analysis/demo/ac6-demo-normal-draw-provenance-guard-v1.json`.
