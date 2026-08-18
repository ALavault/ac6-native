# A properly-timed reachability atlas closes `3859edac`'s retraction: all
# five candidates stay unreached

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. One live probe run using the `probe` CLI's own
`--atlas`/`--xam-movie-record` flags (`ac6-demo-reachability-atlas/v1`),
neutral store, the `634cff33`/`651e7878` press-and-release tuple
(`--input-at 3000,16,0,0,0,0,0,0,1 --input-at 3001,0,0,0,0,0,0,0,1`),
`--until frontend --max-ticks 8000`. Atlas self-hash
`cb2f5fe3…6a725f`, built over replay `f0a8114e…9e59176d` /
`572a59a1…f37c3e370820`. No source change.

## What this closes

`3859edac` retracted the "unreached" claims in `03179c5b`, `8fba5b45`, and
`ce065acb` because all three checked candidate functions against
`buttons16.atlas.json`, whose own metadata bounds it to `tick_range {first:
0, last: 252}` — before the title screen exists (tick 2429) and long before
a correctly-timed START press (tick 3000). It left "generation of a
properly-timed replacement atlas... in progress" as the open item.

This run is that replacement: `tick_range {first: 0, last: 7999}`, covering
both the natural title cycle and a correctly-timed press. `functions`
contains 2779 entries — the identical count `f0e1ad75` (a sibling
12,000-tick run) already reported, cross-validating the mechanism.

## Result: all five retracted candidates are genuinely unreached

Checked by exact address membership in the atlas's `functions` list:

```
0x820C2CC0  NOT REACHED   (8fba5b45's render-state seed-writer candidate)
0x8219D4E8  NOT REACHED   (8fba5b45's seed-writer's own callee)
0x8217C678  NOT REACHED   (ce065acb's CModeTaskGame* delegate-assign method)
0x82173DF0  NOT REACHED   (ce065acb's delegate-store callee)
0x8217C4D8  NOT REACHED   (ce065acb's CModeTaskGame*-family callback target)
```

Two controls, both as expected:

```
0x821AD378  REACHED  first_tick=0 last_tick=7999 count=7863   (the per-frame
                                                                 render-state
                                                                 dispatcher,
                                                                 cf7116b2)
0x821AD7C0  NOT REACHED   (a6e4c5cc's character-parser correction, confirmed)
0x821ADAB8  NOT REACHED   (3e0c76d0's CX360UnitManager event callback)
```

`8fba5b45`'s and `ce065acb`'s original findings were correct in substance;
only their method (a truncated atlas) was invalid, exactly as `3859edac`
diagnosed. On sound evidence, the same conclusion holds: **within the
currently-reachable code, even including a correctly-timed START press, no
seed writer exists that could ever move `[0x827AD2F0]` into `[11,19]`, and
`CX360UnitManager`'s event callback is not reached either.**

## One level up: `sub_820C2CC0`'s own caller is also unreached

`sub_820C2CC0` has exactly one static `bl` caller in the generated code,
`sub_820B2418` (`ppc_recomp.2.cpp:9299`), and no direct table reference to
itself. `sub_820B2418` is **also absent from the atlas.** It has no static
caller either; two table hits reference it (`0x820057DC`, `0x82077EA0`).
`whose_vtable.py` names the first as `CEffectGeneratorAccomBulletRandom`'s
own RTTI locator at vtable slot `+0x48` -- a weak, value-based match
(`whose_vtable.py` scans for aligned words equal to the target address
without semantic verification) that would place this seed-writer chain
inside a gameplay effects class, not frontend code, if it holds. Not
pursued further this cycle; walking past one level is enough to note the
lead without asserting it.

## New, narrower finding kept from the retracted draft: the adjacent field

`8fba5b45`'s exhaustive search covered writers of `[0x827AD1C8+296]`
(`0x827AD2F0`) specifically. Reading `sub_821ADAB8` (`3e0c76d0`'s
CX360UnitManager-armed callback) shows it also sets/clears a bit at
`[0x827AD1C8+300]` (`0x827AD2F4`, computed as `lis r11,-32133; ...
PPC_STORE_U32(ctx.r11.u32 + -11532, ...)`) -- an untouched field
immediately adjacent to the render-state word, not covered by `8fba5b45`'s
`+296`-specific search. Separately, the same function's stores to
`[r31+21600]`/`[r31+21592]`/`[r31+21596]` are `3e0c76d0`'s own "arms
`device+0x5460`" in the vocabulary that report already used --
`0x5460 == 21600` decimal exactly, so this is a units confirmation joining
the two reports' terminology, not a new claim. Since `sub_821ADAB8` is
itself confirmed unreached above, neither field is actually written on the
current route; this is static structure, not a live effect.

## Retracted from this report's own draft

An earlier draft of this cycle's work claimed `sub_821AD378`,
`sub_821AD7C0`, and `sub_821ADAB8` were sibling entries in one runtime class
dispatch table, based on a flat-image scan finding them eight bytes apart
with `{address, 0x4000xxxx}` pairs. Checked before writing anything further:
decoding the second word as `0x40000000 | (length_in_words << 8) |
prologue_words` reproduces every entry's *own* function length exactly --
each entry's computed end address equals the next entry's start address,
for all eight entries scanned. This is the image's `.pdata`
`RUNTIME_FUNCTION` table (ascending address order, per-function
length/prologue metadata), not a class dispatch table -- exactly the table
`cf7116b2` already noted these functions are absent from by name
(`whose_vtable.py` excludes `.pdata` by design). Adjacency in `.pdata` means
nothing about class membership. No claim from that draft survives except
the two items in the section above, re-derived independently of the false
premise.

## Consequence for the plan

`8fba5b45`'s priority conclusion is now soundly re-established: nothing in
the currently-reachable code, on either the forced-argument route or a
correctly-timed natural START press, can construct `CX360UnitManager`,
raise its event, or seed the render-state word. `5dc58584`'s standing
priority is confirmed rather than merely re-asserted. The properly-timed
atlas itself is a reusable artifact/method for closing future "is X
reached" questions on this route without re-deriving reachability by hand.

## Not established

- Whether `CEffectGeneratorAccomBulletRandom`'s vtable-slot identification
  for `sub_820B2418`'s reference at `0x820057DC` is real or a
  value-coincidence; not cross-checked against a second signal.
- What (if anything) legitimately calls `sub_820B2418`/`sub_820C2CC0` in a
  route that does reach `CModeTaskGameDemoOffline` or mission content --
  this atlas only covers the frontend/attract route, not a mission-loaded
  one.
- What, if anything, would make `CX360UnitManager` construct at all --
  `f03eed93`'s seven roots remain the next static target, per that report's
  own standing next step.

## Process note

Before drafting, `git log -S`/report-grep found `a6e4c5cc`, `03179c5b`,
`8fba5b45`, `ce065acb`, and `3859edac` -- five commits this session's memory
file had not yet absorbed, all already on `HEAD`'s ancestry. The false
"shared dispatch table" claim in this cycle's own first draft was caught by
`advisor` before commit, via the `.pdata` arithmetic check above, and is
recorded here rather than silently dropped.
