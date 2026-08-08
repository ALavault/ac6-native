# Cycle 1151 — the single-level surface rule, exactly; the mip chain, not yet

## What was open

Cycle 1150 named four descriptor fields by derivation — width at `+0x14`, height
at `+0x16`, format code at `+0x13`, mip count at `+0x11`, cube flag at bit 9 of
`+0x1C` — and left the surface layout open, on the grounds that a decoder needs
it and guessing it would be the same mistake one field later.

Half of it is now closed by measurement, and the closed half is the half a
Mission 01 decoder actually needs.

## The rule

For a block-compressed texture with **one level and no cube flag**:

```
payload = pad32(ceil(W/4)) * pad32(ceil(H/4)) * bytes_per_block
```

where `pad32` rounds up to a multiple of 32 blocks — the Xenos tile — and
`bytes_per_block` is 8 for BC1 and 16 for BC2/BC3.

```
single-level block textures                          308
payload matches the rule exactly                     308
mismatches                                             0
distinct shapes                                       38
of which non-power-of-two                             26
```

**Zero mismatches over 308 wrappers.** The 26 non-power-of-two shapes are what
make this a test rather than an identity: for a power-of-two texture `pad32` is
a no-op and the rule would be unfalsifiable. It is the odd shapes that pin it —
`120×720` pads 30×180 blocks to 32×192 and predicts 98,304 bytes, which is what
the file holds; `600×424` pads 150×106 to 160×128 and predicts 327,680;
`800×720` pads 200×180 to 224×192 and predicts 688,128. Each is exact.

This is the first quantitative statement about NTXR pixel data in this workspace
that is neither visual nor correlational.

## What it does not settle

**Orientation.** `pad32(a) * pad32(b)` is symmetric, so this rule cannot say
which half of the descriptor is width. That was settled separately and by
derivation in cycle 1150 — `0x8234B360` reads `+0x14` and `+0x16` into distinct
out-slots, and `0x8234EC38` passes the first as `0x821FBE30`'s first argument.
Size arithmetic was never going to answer it, which is exactly what cycle 1149's
null orientation test found the hard way.

## The mip chain is still open, and here is its shape

Grouping the 656 BC3 wrappers by declared shape, the multi-level groups are
internally consistent — every wrapper of a given shape and mip count has the
same payload — but no single per-level padding rule I tried reproduces them:

| W × H | mips | payload | payload / (W·H) | naive tile model predicts |
|---|---:|---:|---:|---:|
| 4096×4096 | 13 | 22,413,312 | 1.3359 | — |
| 1024×1024 | 4 | 1,392,640 | 1.3281 | — |
| 512×512 | 10 | 393,216 | 1.5000 | 458,752 |
| 256×256 | 9 | 131,072 | 2.0000 | 196,608 |

Large textures sit just above the unpadded chain's 4/3, small ones are inflated,
and the naive model — pad every level to 32×32 blocks — **overshoots** in both
checked cases. Applying it to the whole corpus scores 312 of 670, and every one
of the 312 is a single-level texture. It adds nothing over the rule above.

The residuals are structured rather than noisy: the 512×512 and 256×256 tails
are 131,072 and 65,536 bytes, both exact powers of two, which says the levels
below some threshold are allocated at a fixed granularity. I tried a 32×16-block
minimum, which fits the 256×256 tail as 8 levels of 8,192 and then misses the
512×512 tail by one level. I am not asserting a rule on that basis.

## Why this is enough to matter and not enough to finish

Mission 01's atlases are the single-level population — 308 of the 670
block-format wrappers carry one level, including every non-power-of-two UI and
cockpit shape. A decoder built on the rule above can address their pixels
correctly today. It would silently mis-address the mip tail of the 4096×4096
terrain atlases, which is precisely where JG will look.

## Decided rather than asked

Still no C++ decoder, and this is the third cycle in a row to decide that. The
reason has changed each time and that is the point: cycle 1149 lacked the field
names, cycle 1150 had them and lacked the layout, and this cycle has the layout
for one population and not the other. Writing the decoder now would mean either
restricting it to single-level textures — which is defensible and narrow — or
guessing the tail.

The narrow version is the right next step and it is a decision for the cycle that
takes it, with the population boundary stated in the source and in the contract.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  24/24 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  mission01_final_gate=audit-valid JF=pass open=none
audit_ac6_class_map.py ... --require J2              ->  class_map=pass vtables=811 rejects=1619
```

No product code changed.
