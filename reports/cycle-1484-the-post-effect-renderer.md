# Cycle 1484 — the post-effect renderer

## Qualification

- **No Ghidra run and no oracle pass.** The image and `analysis/class-map.tsv`.
- No product C++ changed; ctest stays **60**. **No contract entry.**

## The global has a name

`0x829B9C50` is past the end of the image, so it is `.bss` and runtime-built.
`find_materialised_address.py` gives twelve sites; `0x823D1E04` is in the
static-initialiser range where `CMapManager`'s own global constructor lives, and
it calls `0x8211E8A0` with `r3 = 0x829B9C50`.

That constructor stores its vtable at `0x8211E8F4`:

```
0x8211E8D8  lis  r11,-0x7DFA        \  r11 = 0x8205CF78
0x8211E8E4  addi r11,r11,-0x3088    /
0x8211E8F4  stw  r11,0x0(r31)
```

> **`0x829B9C50` is a `CX360PostEffectRenderer`** (vtable `0x8205CF78`).

## And it builds the whole chain inline

The same constructor materialises four more vtables and writes them into fields
of itself:

| vtable | class |
|---|---|
| `0x8205CF50` | `CX360DOF` |
| `0x8205CF58` | `CX360Fade` |
| `0x8205CF60` | `CX360ChromaticAberration` |
| `0x8205CF70` | `CX360HDR` |

with two more of the family beside them in the table: `0x8205CF68`
`CX360ToneCorrectionVignetting` and `0x8205CF1C` `CX360Noise`, all deriving from
`CX360PostEffectBase` (`0x8205CF48`).

And the ACE6-side family, from the class map:

```
0x82054E0C  ACE6::CAce6PostEffectBase
0x820569FC  ACE6::CAce6HDR
0x82056A14  ACE6::CAce6ReductionBuffer
0x8205C974  ACE6::CAce6Vignetting
0x8205C980  ACE6::CAce6Sun
0x8205C98C  ACE6::CAce6LensFlare
0x8205C998  ACE6::CAce6ToneCorrection
```

## Which answers the reviewer with names

> "Il manque tous les effets de lumières/shader."

The effects are not missing from the game; they are missing from this
reconstruction, and now they have names. **The mapset XML's groups map one to one
onto classes**:

| mapset group | class |
|---|---|
| `.HDR` (10 values) | `ACE6::CAce6HDR` / `CX360HDR` |
| `.Vignetting` (3) | `ACE6::CAce6Vignetting` |
| `.LensFlare` (40) | `ACE6::CAce6LensFlare` |
| `.LevelCorrection` (15) | `ACE6::CAce6ToneCorrection` |

Cycle 1481 implemented three of those four from the file's numbers with curves it
invented. The curves now have somewhere to be derived from.

## And it revises cycle 1483's caveat

Cycle 1483 established that the map loader loads `tone%s.xml` and frees it, and
left open whether the values reach runtime. They have a consumer — a class named
for exactly that job — so "the tone values are unused" was never the right
reading of a discarded buffer. What the loader discards is the *file*; what
`CAce6ToneCorrection` reads has not been traced.

The map loader also registers **three of its own sub-objects into this renderer**
— `[global+0x1388] = this+0x79518`, `+0x138C = this+0x7956C`, `+0x1398 =
this+0x79604` — so the map hands the post-effect renderer three things, and
`+0x7956C` is the sub-object cycle 1462 mistook for the map manager itself.

## Not established

- Everything about how the effects are parameterised at runtime. Six named
  classes and a renderer; not one of their functions has been read.
- Which of the three registered sub-objects is which.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 60
tools/tests                             Ran 79 tests, OK
```

## Next

**`CX360HDR`, because cycle 1481 already implemented it by guess.** Its vtable is
`0x8205CF70` and the mapset gives it ten values whose names — `fBrightPassThreshold`,
`fBloomSigma`, `fBloomScale` — are the parameters of a bloom this repo now has in
invented form. Reading one class turns three invented curves into derived ones,
and it is the shortest path from "looks like a game" to "is this game".
