# Cycle 1227 — the enumeration I named three times and finally ran

## Why this cycle exists

Cycle 1217 closed the vertex-stride derivation and listed, among what it had not
established, *"writers of `[0x82871084]` are not exhaustively enumerated"*. Cycles
1225 and 1226 each repeated it as outstanding. Naming a gap three times without
running it is a way of looking rigorous while doing nothing, so this runs it.

## The field

`[0x82871084]` is the D3D device pointer on the renderer object at `0x82871080`.
It matters because cycle 1217's whole reachability argument for the vertex-stride
table rests on it: the table's builder runs eight instructions before the store
that publishes this word, and the draw at `0x82364518` loads exactly this word
(`8236453c lwz r31,0x4(r25)`) and hands it to `SetVertexDeclaration`. **If
something else writes the device, the "cannot be unbuilt" argument weakens.**

## Two forms, both scanned

A write reaches the field either through a register already holding
`0x82871080` — displacement `0x4` — or by materialising `0x82870000` and using
displacement `0x1084`.

The second, force-scanned over the whole of `.text`:

```
scanned=852724  already_listed=846087  forced=6637  hits=8
822c48d8  stw r9,0x1084(r3)    in none
822c4de4  stw r9,0x1084(r3)    in none
822c4f90  stw r10,0x1084(r3)   in none
822c4fa4  stw r10,0x1084(r3)   in none
822c52e4  stw r10,0x1084(r3)   in Function_822C5138
```

**Five stores, four of them in the unanalysed gap** — invisible to every scan this
repository ran before cycle 1221.

The instrument's control on the first form passed in the same session: scanning
for `0x1080` finds `8234f7c4 addi r3,r11,0x1080` **in none**, which is the thunk
`0x8234F7C0` cycle 1217 named. The function that carries the known chain is
itself in the gap, which is why the enumeration was never going to close by
ordinary means.

## What the answer actually is

Cycle 1217's claim moves from **"not exhaustively enumerated"** to **"bounded to
seven candidates, five of them unresolved"**: the two `+0x4`-alias writers cycle
1217 found, plus these five.

That is a smaller claim than "the field has one writer" and a much larger one than
"unknown". It is also enough to say what it does to the stride argument:
**nothing yet.** All five candidates are on `r3`, and whether `r3` holds
`0x82870000` at any of them is unread. They sit in `0x822C4xxx`–`0x822C5xxx`,
nowhere near the renderer code, which is weak evidence against and not evidence.

## Not established, stated plainly

- The base register at each of the five. That is the whole remaining question and
  it needs five short disassemblies I did not spend, because the honest unit of
  work here was the enumeration, not the resolution.
- Whether the `+0x4` form has writers beyond the two cycle 1217 found. Scanning
  `,0x4(` across `.text` returns a set far too large to be useful, and I have no
  way to filter it by base without dataflow.
- Therefore cycle 1217's "the table cannot be unbuilt on any path where the draw
  has a device" **stands as written but is not strengthened by this**. It rests on
  the eight-instruction adjacency, which is unaffected.

## A note on the shape of this session

Twelve of the last twenty cycles have been corrections, voided negatives, or
enumerations that turned out bounded rather than closed. That is what happens
when the instruments improve faster than the findings: **each new tool re-opens
what the previous one had settled**, and the settled things were settled at the
old tool's resolution.

The alternative would have been to stop building tools. Cycles 1198, 1208, 1214
and 1218 show what that costs.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
852,724 instructions, 6,637 forced, 8 hits, 5 stores
```

No product code changed.
