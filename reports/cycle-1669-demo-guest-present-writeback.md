# Cycle 1669 — writeback `XE_SWAP` vers `GuestMemory`

## Verdict

Le resolve atteint est maintenant relié, de façon transactionnelle et
fail-closed, à l’allocation guest portée par `XE_SWAP`. Le résultat Vulkan
tiled est relu depuis l’allocation guest après écriture et son image linéaire
reproduit exactement le digest du resolve host. Cette preuve ferme le
transfert renderer→mémoire guest pour le frame neutral atteint ; elle ne
prouve encore ni lecture par un consumer guest, ni pixels non noirs, ni
screencap promouvable.

## Cible et contrat

- cible exclusive : `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` ;
- Xenon big-endian / Xenos ; aucune preuve retail fusionnée ;
- `XE_SWAP` observé : adresse `0x1374A000`, format 6, tiled, 1280×720 ;
- extent tiled exact : `0x398000` octets, fin exclusive `0x13AE2000` ;
- source générique de tiling : ReXGlue `cb58065c793429aa92895d778af58d12e9d26d8f`,
  déjà ported dans `src/xenos_tiling.cpp` ; ce checkout n’est pas une preuve
  AC6.

## Garde et effet

`src/xenos_guest_present_join.hpp` refuse toute variation d’adresse, de
dimensions, d’extent ou de résultat `present_joined`. `DemoSession` exige que
la plage complète soit à la fois mappée et couverte par une allocation
enregistrée par `GuestBridge`; sinon `RuntimeTrap` est levé.

Le writer lit d’abord la plage guest existante, remplace uniquement les quatre
octets de chaque pixel selon `reached_rgba8_tiled_offset`, puis appelle
`GuestMemory::store_bytes`. Les octets de padding ne sont donc pas remplis par
le canari Vulkan. La plage est ensuite relue, untiled, et comparée au digest
linéaire host avant que `guest_writeback` soit publié.

## A/B frais, 253 ticks, backend Vulkan

Les deux routes ont utilisé le binaire codegen ON
`d5e5dbcfe49d4b2ab1392b4d49b72cbb4a9f51320be86f3c263233ad87a74714` et le
store démo PAL neuf `.build/ac6-demo-store-test-3`.

| route | RTPLY SHA-256 | résultat | `guest_writeback` | digest guest linéaire |
|---|---|---|---:|---|
| neutral | `c5357c6d9639c2a675161bd10c3cdbe97096df0dac11d760fe8a2d964b1c5794` | `play`, rc 0 | 1 | `0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f` |
| START tick 252 | `4a7326d9b1148dc6a5943cabea5c0e2562ae7ad833eda6bc7cd92a089e25724f` | `probe`, `max_ticks`, rc 4 attendu | 1 | `0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f` |

Le digest host linéaire reste
`0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f`.
Le digest guest tiled relu est
`87898167ad53aa9cf9f8a867de7becf9fa422d5f3fc5c65710a9b67ad7ec1f3d` ; il
diffère du digest tiled host parce que le host conserve un canari dans les
octets de padding non écrits.

Avec `AC6_DEMO_WATCH_FRONTBUFFER_WRITERS=1`, chaque route a produit une seule
écriture host vers `0x1374A000`, taille `3768320`, caller module offset
`0x9BA87`. C’est le writer renderer `GuestMemory::store_bytes`, pas un store
PPC guest inventé ; aucun PC/LR guest ni consumer guest n’est attribué à cette
ligne.

## Validation

- CTest codegen ON : 17/17 ;
- CTest démo OFF : 18/18 ;
- audit complexité/source : pass ;
- normal readback :
  `0b150fd32588b1daca5569992ebe559c0102c837306b1af4c44d35128ec58366` ;
- resolve host tiled :
  `94831d4c398252020f792d92f546c5122ad522c4270b73be9e8619fde1db641f` ;
- RTPLY neutral (writer-instrumented run) :
  `c5357c6d9639c2a675161bd10c3cdbe97096df0dac11d760fe8a2d964b1c5794` ;
- RTPLY START (writer-instrumented run) :
  `4a7326d9b1148dc6a5943cabea5c0e2562ae7ad833eda6bc7cd92a089e25724f` ;
- rapports et traces bruts conservés seulement sous `/fastdata/lavaulta/tmp`.

## Classification

- `demo-qualified` : destination `XE_SWAP`, allocation guest complète,
  writeback pixel-only, relecture guest et digest linéaire A/B identique ;
- `xenia-generic` : formule de tiling et bornes, via la source ReXGlue épinglée ;
- `demo-observed` : frame atteint noir et séquence neutral/START à 253 ticks ;
- `unknown` : premier consumer guest, persistance sur deux ticks observée par
  le code invité, pixels non noirs, transition frontend, mission et screencap.

Aucun fallback visuel, aucune mutation retail/Ghidra/Xenia/ReXGlue/C++ généré
ou microcode suivi n’a été introduit.

## Prochain checkpoint

Instrumenter les lectures guest de `[0x1374A000,0x13AE2000)` après le writeback,
avec adresse, largeur, PC/LR, thread et tick, puis refaire neutral/START. Ne
promouvoir une screencap que si un consumer guest réel relit les bytes et que
le digest persiste sur au moins deux ticks.
