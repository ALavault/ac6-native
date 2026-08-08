# Cycle 1180 — the route lead, opened and left open

## The link the debt page already had

`WORLD_POSITION_DEBT.md` records, from earlier work: *"unit+0xE0 is the Obj list,
unit+0xF0 the cursor into it, unit+0xC0 the destination. Vtable +0x34 selects
entry N … and 0x822A23D8 resolves the entry's Param block by a mode byte +0x2A
with an anchor pair at +0x2C/+0x2D — the same record shape as the tag-2 order at
other offsets."*

Put beside cycle 1179's result — `PLAD`'s only effect is to write `+0xF0`, the
cursor — that is a chain worth following: the player slot picks a starting index
into the unit's **own Obj list**, and a route entry's Param block has the shape
`0x822953F0` resolves. Mission 1's `PLAD` word 3 is `0`, so the cursor starts at
entry 0.

## What the player's entry actually holds

```
player unit          2 record children
Obj entries          1
  entry 0            2 children, own data present
    child 0          data present, 7 grandchildren
                     first words: 00000000 46F00000 00000000 00000000 …
    child 1          no data, no children
```

`0x46F00000` is `30720.0`. It sits at `+0x04` of the ObjBin block — the same
block that carries the model bytes at `+0x61`/`+0x62`, which for the player are
both the `0xFF` sentinel (cycle 1176).

## What I am not concluding

That 30720 is an altitude, or a position component, or anything. It is one float
in a block whose layout is known at four offsets out of ninety-eight, and the
seven grandchildren under it have not been opened. A number that looks like a
plausible altitude is exactly the kind of evidence cycles 1146 and 1179 spent
themselves refusing.

The mode byte at `+0x2A` and the anchor at `+0x2C` — the fields that would make
this a resolvable Param block — read 0 and 0 on the child itself, which is not
the same as finding them on the right sub-block.

## Where it stands

The lead is real and it is one level deeper than this cycle went: the seven
grandchildren of the player's Obj child. That is the next read, and it wants a
fresh pass rather than the tail of a long one — the three windowed-reading errors
this session all came from pushing one step further on a spent context.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed.
