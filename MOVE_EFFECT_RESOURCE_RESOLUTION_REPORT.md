# MoveEffect resource-resolution boundary

Date: 2026-07-15

## Static trace result

`CDemoMoveEffect` has an MSVC type descriptor at `0x826e87ac` (the type-name
string starts at `0x826e87b4`). Its class-hierarchy data is present at
`0x82073698`, but the local XEX analysis exposes no direct code reference,
factory, vtable, or resource lookup from that RTTI data.

The already recovered NFIC runtime dispatcher at `0x8236b920` sends each
serialized event through a dynamically supplied receiver vtable slot. It does
not identify a concrete receiver for tag `0x0101`, so it cannot prove a
renderer, parameter decoder, lifetime, or an effect factory.

## Exact archive join

The local remaster export has 16 CUTs containing `MoveEffect`. Across their
60,626 records, all 247 cut-local distinct effect IDs resolve by this exact
one-based join:

`MoveEffect.effect_id - 1 == Scene record index == adjacent resource-FHM member index`

Every resolved path in these samples contains `/E_EFFMOVE_` and ends in a
`.mop` resource. For example, CUT `0002` uses IDs 4, 5, and 7; its Scene
records 3, 4, and 6 name the three corresponding E_EFFMOVE MOPs. The check has
no misses across all 16 CUTs.

This establishes resource association only. It does not establish MOP effect
semantics, playback duration, transform evaluation, visibility, or rendering.

## Native boundary

`resolve_move_effect_scene_resource` in
`reconstruction/ace-combat-6/src/scene.cpp` performs only the evidenced
one-based Scene/resource-FHM join. It rejects zero IDs, observed-nonzero flag
halves, out-of-range resources, and paths outside the exported E_EFFMOVE form.
It has no visual side effects.

`ac6-scene-tests` covers the successful association and rejects a non-zero
flag or a non-E_EFFMOVE path. The full AC6 CTest suite passed 36/36 after this
addition.
