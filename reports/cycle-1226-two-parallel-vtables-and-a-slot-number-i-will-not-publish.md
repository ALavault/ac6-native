# Cycle 1226 — two parallel vtables, and a slot number I will not publish

## What is directly observed

`0x821B54C0` and `0x821B5808` — the fixed and the general mode-creator setters —
sit in two **byte-parallel** runs of `.rdata`:

```
820654e0  822ddbe8 821b04d0        82065590  822ddbe8 821b5690
820654e8  821adfb0 821b7948        82065598  821adfb0 821b7948
820654f0  821b7958 8218d2a0        820655a0  821b7958 8218d2a0
820654f8  8218d2c8 8218d2f0        820655a8  8218d2c8 8218d2f0
82065500  821b5398 821ae048        820655b0  821b5540 821ae048
82065508  8218d348 8218d360        820655b8  8218d348 8218d360
82065510  8218d480 821af2c0        820655c0  8218d480 821af2c0
82065518  8218d598 821b54c0   <--  820655c8  8218d598 821b5808   <--
82065520  82077634 821b54d8        820655d0  820776a0 821b58e8
```

Fifteen entries, **identical except at four positions**, and our two functions
occupy the same one. They are overrides of a single virtual method on sibling
classes: the left version fixes the creator to table entry 0, the right takes an
index argument (cycle 1224).

Three other slots differ in the same way — `821b04d0`/`821b5690`,
`821b5398`/`821b5540`, `821b54d8`/`821b58e8` — so this is a small override set,
not two unrelated tables that happen to overlap.

## The number I am not publishing

Both functions sit `0x3C` from the start of their block. That is a tempting slot
index and I am not asserting it, because **the blocks' starts are not
established.**

The check that should have settled it fails. MSVC puts a
`RTTICompleteObjectLocator` at `vtable − 4`; here:

```
[0x820654DC] = 0x821D10C8      a .text pointer
[0x8206558C] = 0x821D10C8      the same .text pointer
```

A locator points into `.rdata`. **These do not**, so either RTTI is off for these
classes or `0x820654E0` and `0x82065590` are not vtable starts — sub-object
boundaries inside a larger table would look identical from here.

The structure before each block repeats too — `00000000`, then a
`{0x820776xx, 0x821Bxxxx}` pair, then the identical `821d10c0 821d10c8` — which is
consistent with either reading and discriminates neither.

So: **the two functions are at the same offset in two parallel runs.** That is
what I measured. "Slot `+0x3C`" would be that measurement plus an assumption
about where the run begins, and cycle 1225's own success came from a tool that
made such assumptions unnecessary.

This is the fourth time today a verification step stopped a number before it was
written down — after 1216's composition rule, 1211's stride control and 1203's
aggregation. It is also the first where the verification *failed* and the honest
output is a smaller claim rather than a bigger one.

## Not established, stated plainly

- The vtable starts, hence the slot index, hence what a caller would look like.
- The class names. Cycle 1218's RTTI recovery found 811 named vtables; **it does
  not reach these**, and that is information about the technique's coverage as
  much as about these classes.
- Who calls the slot. Unchanged from cycle 1225, and now known to need the start
  first.
- Cycle 1216's `[0x82871084]` enumeration, named as outstanding in three
  consecutive cycles and still not run.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
two 15-entry runs compared entry by entry; [start-4] read for both
```

No product code changed.
