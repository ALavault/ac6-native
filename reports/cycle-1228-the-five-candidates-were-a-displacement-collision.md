# Cycle 1228 — the five candidates were a displacement collision, and the enumeration closes

Cycle 1227 bounded the writers of `[0x82871084]` to seven candidates and left
five unresolved, saying the resolution needed five short disassemblies it had not
spent. Here they are.

## What the five actually are

```
822c48c8  stw  r9,0x0(r11)      ; a 0x80-iteration clear loop
822c48d4  stw  r9,0x1074(r3)
822c48d8  stw  r9,0x1084(r3)    <- candidate
822c48dc  blr
```

```
822c52c0  stfs f0,0x1058(r3)
822c52c4  stw  r10,0x1050(r3)
822c52c8  stfs f13,0x105c(r3)
822c52cc  stw  r10,0x106c(r3)
822c52d0  stfs f13,0x1060(r3)
822c52d4  stw  r10,0x1070(r3)
822c52d8  stfs f13,0x1064(r3)
822c52dc  stw  r10,0x1074(r3)
822c52e0  stfs f0,0x1068(r3)
822c52e4  stw  r10,0x1084(r3)   <- candidate
822c52e8  stfs f0,0x1078(r3)
```

Both sit inside a **contiguous field-initialisation block** — displacements
`0x1050` through `0x1084` on one `r3`, with `stfs` and `stw` interleaved.

**`+0x1084` on the renderer is a D3D device pointer. It does not live in a block
of single-precision floats.** Six of its neighbours here are `stfs`. The base is a
function argument, and the object being initialised is not the renderer.

## The enumeration closes

All five absolute-form candidates are a **displacement collision** — a different
object with fields at the same offsets. So for that form the answer is **zero
writers**, and cycle 1217's enumeration reduces to the two `+0x4`-alias writers it
already had: the constructor's zero and `8233e704`, the device publication.

Cycle 1217's reachability argument — *the stride table cannot be unbuilt on any
path where the draw has a device* — is now **supported rather than merely
unrefuted**. Nothing else writes the field in the form that could be scanned.

## The third collision

`0x1AD8` in cycle 1220, and now `0x1084`. Both times a displacement scan produced
a clean candidate list, and both times the candidates were a different structure
at the same offset.

The tell is available in both cases and is cheap: **read the neighbours.** A field
belongs to the structure its neighbours belong to. `0x1AD8` was surrounded by a
freshly allocated pointer; `0x1084` is surrounded by floats. Neither needed
dataflow, register tracking, or a second tool — just four lines of context that I
did not print the first time.

That is a better rule than "beware collisions", because it says what to do.

## Not established, stated plainly

- Whether the `+0x4`-alias form has writers beyond the two known. Unchanged from
  cycle 1227: scanning `,0x4(` returns a set too large to filter without
  dataflow, and this cycle does not touch it.
- What the object at `0x822C4xxx`/`0x822C5xxx` is. It has floats at `+0x1058`
  through `+0x1078` and was not identified; it did not need to be.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
five candidates read in context; all five neighbours-of-floats
```

No product code changed.
