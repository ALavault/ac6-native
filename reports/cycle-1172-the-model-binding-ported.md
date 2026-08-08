# Cycle 1172 — the model binding, ported, and it is the ladder's risk item

## What was written

`ScenarioModelBinding` in `include/ac6/retail_scenario.h`, one per Obj record,
parsed in `src/retail_scenario.cpp` from the block `0x82330158` stores — the Obj
entry's **child[0]**, not the entry.

```
bindings                       434
without a model (0xFF)         123
carrying a secondary           309
secondary == primary + 1       281
distinct primaries              38
highest primary                 74   (the directory has 94 entries)
```

The parse reproduces the raw measurement of cycle 1171 exactly, and the parser
test asserts every one of those numbers plus a structural rule that comes from
the code rather than from the data: `0x820A7944` tests the primary against `0xFF`
and skips the entire model block on the sentinel, so a record without a primary
must not carry a secondary. It never does.

## Why this is the item the ladder warned about

`MISSION01_LADDER.md` has said since the plan was written that step 2f is *"where
JV would quietly become J1 again"*, because a hand-written class→model table
satisfies the auditor and destroys the property the auditor exists to protect.

This is that step, and the reason it is not that failure is that no table was
written. The chain is read end to end:

```
unit record
  -> Obj entry             (0x8232F380 builds the list, 0x8232F198 fills it)
  -> child[0] data block   (0x82330158, ObjBin::read)
  -> bytes +0x61 / +0x62   (0x820A7944, 0x820A795C, 0x820A7968)
  -> 0x8228E9B8            (entry = base + table[index], bound by [blob+0x04])
  -> the container         (0x820A85E0 via vtable +0x0C, written by 0x8228E988)
  -> Mission 01's MDLP     (94 entries, +0x0C table, +0x10 base)
```

Every link is an instruction in the header, and the auditor reads them out of the
file. Nothing in it says "class 2 means an F-16".

## What is carried and not interpreted

The pair. `secondary` is `primary + 1` in 281 of 434 records and the directory
entries are consecutive, so it is tempting to call it a variant, a damaged model,
a level of detail or a shadow proxy. **None of that is established**, and the
struct carries the two bytes with the sentinel semantics the code gives them and
no further meaning. `has_secondary()` says whether the second lookup happens; it
does not say what the second entry is for.

## What this does not do

It does not load a model. `MissionRuntime` still resolves no asset, the retail
session still declares `mission_ready` false, and nothing is drawn from a
bundle. What exists now is the index and the directory it addresses — the join,
not the loader.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
audit_ac6_class_map.py ... --require J2              ->  class_map=pass 811/1619
```
