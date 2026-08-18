# Correcting `e3d7eca4`: a mispaired argument missed the first call; a live bracket pins the exact offset

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`). XEX
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. One live run against
`recompilation/ace-combat-6-demo/build-codegen-on/ac6-demo-recomp probe`, same
neutral store and injection recipe as every run in this thread
(`AC6_DEMO_INJECT_ENDMODE_AT_TICK=2571`, `AC6_DEMO_INJECT_ENDMODE_OFFSET=0xE0C`,
`--input-at 3000,16,...`), reproducing the identical trap
(`outcome.kind=trap`, `completed_ticks=2571`). Plus a corrected re-read of
`ppc_recomp.6.cpp:895-993` (`sub_820E1010`).

## The error

`e3d7eca4` read `sub_820E1010`'s body and paired each `addi r3,r31,N`
instruction with the `bl` that follows it in the listing, concluding there
were **three** calls to `sub_820CF958`, at `+312`/`+336`/`+356`. Re-reading
the same lines line-by-line: the compiler interleaves independent `stw`
instructions (writing unrelated scalar fields via `r30`, not `r3`) between an
argument setup and its call, so the FIRST `addi r3,r31,292` (line 934) was
skipped over — the eye lands on `addi r3,r31,312` (line 953) as if it were the
first, when it is actually the *second*. There are **four** calls to
`sub_820CF958`, not three:

```
r3 = r31+292 (line 934)  →  bl sub_820CF958 (line 951)   ← missed
r3 = r31+312 (line 953)  →  bl sub_820CF958 (line 960)
r3 = r31+324 (line 962)  →  bl sub_820CF540 (line 965)    (different helper, unchanged)
r3 = r31+336 (line 967)  →  bl sub_820CF958 (line 970)
r3 = r31+356 (line 972)  →  bl sub_820CF958 (line 979)
```

Each `addi r3,r31,N` sets up the argument for the call that follows it —
nothing between them writes `r3` — so the pairing is by position, not by
adjacency in the listing.

## The exact answer, live-confirmed

Bracketed the whole candidate region (`AC6_DEMO_WATCH_ADDR_LO=0x2E3E3CD4`,
`_HI=0x2E3E3E40`, covering `[base-312, target+8)` for every offset guessed in
`e3d7eca4`) across the full run instead of guessing a base and checking it.
None of the three previously-guessed bases (`0x2E3E3D00`, `0x2E3E3CE8`,
`0x2E3E3CD4`) ever receives a write of the `ASContext` vtable
(`0x820065A4`) anywhere in the run — confirming they were never real
candidates, just arithmetic artifacts of the missed first call. Grepping the
same log directly for the vtable value instead finds it once:

```
AC6_ADDR_RANGE_WRITE address=0x2E3E3D14 size=4 value=0x820065A4 tick=2451
  thread=1 lr=0x820E6780 function=__imp__sub_820E1010 generated_line=902
```

`generated_line=902` is exactly `sub_820E1010`'s own `PPC_STORE_U32(ctx.r31.u32
+ 0, ctx.r11.u32)` — its own vtable install. **The parent `ASContext`
instance's base is `0x2E3E3D14`.** `0x2E3E3E38 − 0x2E3E3D14 = 292` —
**exactly the first, previously-missed call's offset.** `0x2E3E3E38` is
`sub_820E1010`'s first embedded list-head field (`this+292`), not the second
(`+312`) as guessed.

This base matches neither title's context slot (`0x2E3DFA08`, distance
`0x430C`) nor startup's (`0x2E3BFA08`, distance `0x2430C`) — confirms
`e3d7eca4`'s "a third, previously-unnamed `ASContext` instance" conclusion,
now on the correct, exact address rather than a guessed one.

## What this settles from `e3d7eca4`'s open list

- Which offset `0x2E3E3E38` is: **settled, `+292`**, not `+312`/`+336`/`+356`.
- The parent instance's exact base: **settled, `0x2E3E3D14`**.

## Still not established

- Whether this third `ASContext` instance (`0x2E3E3D14`) is ever title's own
  active dispatch context, or an unrelated sibling — per `bfc927e1`, many
  owner instances funnel through one shared interpreter slot; this one's
  role is unchecked.
- What `sub_820CF540` (the `+324` field, a differently-shaped 3-way
  self-link sentinel calling `sub_820CEFD0()` rather than the generic
  allocator — read incidentally this cycle while re-verifying the pairing,
  not analyzed in depth) actually manages.
- Whether the sentinel node's later disappearance is ordinary allocator
  churn or a genuine missed-update teardown bug — unchanged from `e3d7eca4`.

## Process note

`git log --oneline --reverse e3d7eca4..HEAD` is empty — `e3d7eca4` is still
`HEAD`. This correction was caught by actually running the disambiguating
experiment `e3d7eca4` left open, rather than by re-reading the same static
listing a third time — the live bracket is what exposed the pairing error,
not scrutiny of the source alone.
