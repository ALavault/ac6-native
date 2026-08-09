# Cycle 1485 — twenty-four of twenty-four

## Qualification

- **No Ghidra run and no oracle pass.** The image and the archive.
- No product C++ changed; ctest stays **60**. **No contract entry** — the
  structure is derived but nothing is ported yet.

## `CAce6LensFlare`, and its eight elements

`CAce6HDR`'s vtable `0x820569FC` is materialised inside `CMapManager`'s
constructor:

```
0x820FA42C  addi r10,r11,0x69FC       r10 = 0x820569FC
0x820FA458  addi r11,r11,-0x6A30      r11 = this + 0x795D0
0x820FA464  stw  r10,0x0(r11)
```

> `ACE6::CAce6HDR` is embedded in `CMapManager` at **`+0x795D0`**.

Twenty instructions earlier the constructor calls `0x820F9E78` with
`r3 = this + 0x7956C` — **the function cycle 1462 read and dismissed as "a
different structure", correctly and without a name.** It has one now, and the
proof is arithmetic.

Its stored constants, resolved from the image:

```
+0x04  0.922   +0x08  78   +0x0C  41, 44,129
+0x10 -0.579   +0x14  32   +0x18  44, 50, 50
+0x1C -0.481   +0x20  22   +0x24  27, 27, 27
+0x28  0.2     +0x2C  23   +0x30  68, 60, 64
+0x34  0.4     +0x38  42   +0x3C  75, 56, 52
+0x40  0.5     +0x44  55   +0x48  55, 36, 91
+0x4C  0.6     +0x50  30   +0x54  32, 32,119
+0x58  0.835   +0x5C  25   +0x60  24, 86, 28
```

And Mission 01's mapset XML, `.LensFlare.Lens01..Lens08`:

```
01  0.922/78/41,44,129   02 -0.579/32/44,50,50   03 -0.481/22/27,27,27
04  0.2/23/68,60,64      05  0.4/42/75,56,52     06  0.5/55/55,36,91
07  0.6/30/32,32,119     08  0.835/25/24,86,28
```

> **Twenty-four values. Twenty-four matches. In order.**

So the structure is: eight elements of **twelve bytes** from `+0x04`,
`{float position, float radius, u32 packed colour}`, and `0x820F9E78` is
`ACE6::CAce6LensFlare`'s constructor writing its compiled-in defaults.

Two sources that share nothing — constants in the executable and text in an
archive extracted by a different tool — carrying the same twenty-four numbers in
the same order. Nothing was fitted; the offsets came out of the instructions and
the values out of the file.

## What it settles, and what it does not

**Settles**: the layout, the class, the embedding offset, and cycle 1462's
unnamed structure.

**Does not settle**: that the XML *overrides* the defaults. Matching numbers show
the file documents the same values, not that a parser writes them — and the
parser has not been read. It is the natural reading and it is not derived.

That distinction matters more than usual here, because cycle 1483 established
that the map loader loads `tone%s.xml` and frees it. A file whose values match a
constructor's defaults exactly would look identical whether it is read or
ignored.

## Where cycle 1481 stands now

It implemented HDR bloom, vignette and levels with invented curves. The classes
that own those jobs are named — `CAce6HDR` at `CMapManager+0x795D0`,
`CAce6Vignetting`, `CAce6ToneCorrection` — and the lens flare it never attempted
is now a derived structure. The curves are still invented; the parameters they
would consume are no longer guesses about where they live.

## Not established

- Whether the XML is parsed at all.
- `CAce6HDR`'s own layout, which is the next one of these.
- The other two registered sub-objects, `+0x79518` and `+0x79604`.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 60
tools/tests                             Ran 79 tests, OK
```

## Next

**`CAce6HDR` at `+0x795D0`, the same way.** Its constructor's float constants
should join against the mapset's ten `.HDR.*` values exactly as these
twenty-four did, and if they do, the bloom cycle 1481 invented has a derived
parameterisation and a place to read it from. The method is now a recipe:
resolve the constructor's stored constants, and lay them beside the XML.
