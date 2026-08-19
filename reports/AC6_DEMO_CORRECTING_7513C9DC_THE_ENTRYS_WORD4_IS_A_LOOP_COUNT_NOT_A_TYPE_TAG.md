# Correcting `7513c9dc`: the frame-table entry's `+4` word is a loop
# count, not a type tag — and it precisely confirms the finding

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. One short live probe (`--until frontend
--max-ticks 3200`, `634cff33`'s START tuple, neutral store,
`AC6_DEMO_WATCH_ADDR_LO=0x2DD6A854 HI=0x2DD6A85C`) plus a precise
instruction-by-instruction read of `sub_82323BB8`'s dispatch setup
(`ppc_recomp.43.cpp:16610-16685`), the exact code `7513c9dc` and its
sourcing fork paraphrased rather than transcribed.

## What this corrects

`7513c9dc` (and the fork whose static reading it relied on) described the
frame-table entry at `[[owner+40]+index*8]` as directly supplying a "type
tag" used to pick the third jump table's slot, and named reading that
entry's raw content as the next step. Reading the actual generated
instructions shows this is not quite right, and the correct mechanism is
more precise — and confirms `7513c9dc`'s live-observed conclusion even
more exactly than before.

## The real computation, instruction by instruction

Before the loop (`ppc_recomp.43.cpp:16636-16663`):

```
r11 = [owner+220]              // dispatch index
r9  = index*8 + [owner+40]     // &entry[index]  -- the table entry itself
r11 = LOAD_U32(index*8 + [r26+8])   // a DIFFERENT array, off object r26
r10 = LOAD_U32([r26+0]) + r11        // cursor start value
r11 = LOAD_U32(entry+4)                  // <- the entry's OWN +4 word
STORE [owner+248] = r10                    // cursor stored
if (r11 <= 0) skip the loop entirely
r30 = r11                                     // loop count = entry's +4 word
```

Inside the loop (`loc_82323E30`, `:16664-16685`):

```
cursor = [owner+248]
type   = LOAD_U32(cursor + 0)      // <- the ACTUAL type selector
table_base[type*4]  -> bctrl        // dispatch (table_base = r25 = 0x8264CE6C)
r30 -= 1; loop while r30 != 0
```

**Two corrections**: the entry's `+4` word is the loop's own iteration
count (`r30`), not a type value — confirmed directly, `cmpwi r11,0 / ble`
gates entry to the loop on it, then `mr r30,r11` seeds the counter. And
the actual per-iteration type selector is read from `[cursor+0]`, where
`cursor` is computed from a *separate* object (`r26`, unidentified in
this report) and its own two fields (`[r26+0]`, `[r26+8]`) — not the
frame-table entry's own `+0` word directly.

## What the new entry's content actually tells us

The bytes captured (`sub_82278F78`'s background loader, tick 2436, same
mechanism this campaign already knows fills the `swg` bytecode buffers):
`00 0B 96 20 00 00 00 01`. Word `+0` = `0x000B9620` — not directly
meaningful without knowing `r26`'s own fields (not read). **Word `+4` =
`0x00000001`** — by the corrected reading above, this is the loop count
directly: **the new table specifies exactly one dispatch iteration**,
precisely matching (and now explaining, rather than merely correlating
with) `7513c9dc`'s live observation that the dispatch index is `0` on
every sampled tick after the swap and never advances.

## Reading

`7513c9dc`'s live-measured conclusion is unaffected and, if anything,
sharpened: the new frame genuinely dispatches exactly once per tick (loop
count `1`, directly confirmed from the entry's own data, not merely
inferred from the index staying `0`). What remains genuinely open is
*which* of the third table's 14 slots that one iteration lands on — this
depends on `[cursor+0]`, which depends on object `r26`'s own two fields,
which were not identified or read in this report. Reading the frame-table
entry's raw bytes alone, as `7513c9dc` proposed, does not settle this —
`r26` must be identified first.

## Not established

- What object `r26` is (this owner's own field, a global, or a parameter
  passed into `sub_82323BB8`) and what its `+0`/`+8` fields hold.
- The actual type value dispatched for the new, one-entry frame, and
  therefore which of the 14 table slots (and which function) now runs
  once per tick in place of the old two-entry cycle.
- Whether that one dispatch does anything meaningful (e.g. plays a sound,
  advances toward a real next state) or is a genuine no-op — this is the
  question that would actually distinguish "intended design" from "bug"
  per `7513c9dc`'s own open framing, still unresolved.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean below
before this commit. No source change.
