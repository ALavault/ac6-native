# o_1112

`aigaion.mp4` — 120 frames of MDLP entry 2's largest record, at 1280×720.

**`o_1112_lod1_O_OBJ_O_HIR`, 7,601 vertices, 2409 × 170 × 1082.** A flying wing
with eight engine nacelles and a central hull, 2.4 kilometres across.

Ace Combat 6's aerial fortress is the **P-1112 Aigaion**, and it leads the
attack on Gracemeria in Mission 01. The record's name carries the designation;
the shape and the scale agree with it.

**The identification is a reading**, and it is the same kind cycle 1428 kept
apart from its measurements: `o_1112_lod1` and `2409 × 170 × 1082` are the
file's, "the Aigaion" is mine. What supports it is that the number in the name
is the designation and that nothing else in the game is a 2.4 km flying wing.

## What I had wrong

Three cycles called entry 2 "the terrain", on nothing but its size — 2409 units
against a 15-metre tank. Cycles 1428, 1429 and 1436 all repeat it, and the
roster README's table still lists it that way.

It is not terrain. **Mission 01's package has no terrain model in the roster at
all**, which is a different and more useful thing to know, and it was hidden
behind a label applied once and never checked.

## What is retail's

The whole chain, as `../ndxr-textured/README.md` sets out: geometry, normals and
texture coordinates from the NDXR, the material's first texture id resolved
against a GIDX identifier, and a 2048×2048 base level decoded from it.

## What is chosen

The camera, the light, `swap_16 = true`, `repeat` wrapping, affine
interpolation, and the material's **first** texture — all as
`../ndxr-textured/README.md` records.

Also: **only the `o_1112` records are drawn.** Entry 2 is a bundle of 114
distinct names — `nmbs001..368` and `hire01..` among them — and they share one
local space, so drawing all of them stacks them. Cycle 1437 established that the
record's first 32 bytes are a bounding sphere rather than a transform, so the
stacking is the file's arrangement and not a missing read.
