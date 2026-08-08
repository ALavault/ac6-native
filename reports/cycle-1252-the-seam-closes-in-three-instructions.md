# Cycle 1252 — the seam closes in three instructions

Cycle 1251 named a seam and said it was checkable in one dump: **which class
family the array at `[leader+0xD8]` holds**, given that its owner is an
`ACE6::CAce6Unit` and the object `0x8229ADF8` places is a `galib::CGaObj`.

## The answer

```
820a7b00  or  r4,r31,r31      ; r4 = the loop's object — the CGaObj (cycle 1250)
820a7b04  or  r3,r30,r30      ; r3 = [r1+0x164], the container
820a7b08  bl  0x8226f050      ; append(container, object)
```

and `0x8226F050` takes the object in `r4` (`8226f064 or r30,r4,r4`) and the
container in `r3`. Twenty bytes later the parent pointer is read from **the same
`r30`**:

```
820a7b28  lwzx r11,r11,r30    ; the same container
820a7b2c  stw  r11,0x188(r31) ; -> the CGaObj's parent field
```

**The array holds `CGaObj` pointers.** `[leader+0xD8]` is a slice of it
(`820a7c74` stores `&unitPtrArray[idx+2]`), so a `CAce6Unit` leader owns an array
of `CGaObj` children — and every reading already on record agrees:
`822a28f0` tests `[r29+0x118]`, `+0x118` is a `CGaObj` field, and `0x8229AF80`
reads its pointee with the layout it writes on itself.

The seam was not a contradiction. It was a question nobody could ask while one
noun covered both classes, and it took three instructions to answer once cycle
1250 had made it askable.

## What this closes and what it does not

Cycle 1251's structural gap is closed: steps 8 and 9 operate on the same family,
and the chain is coherent from `0x821F5E90` to `unit+0xA0`.

**It does not touch liveness.** Cycle 1251's load-bearing caveat stands unchanged
— steps 8 and 9 are structure, not reachability, and cycle 1250's warning that *a
true positive from dead code would look exactly like this* applies to this cycle
too. Three correctly-read instructions establish what the array holds. They
establish nothing about whether the append runs in Mission 01.

## Not established, stated plainly

- Everything cycle 1251 listed, minus the seam: the leaf class among the six
  `0x138`-byte vtables; Set 0 as the player's Set; `[unit+0x118] & 0x2` at
  construction; and the liveness of steps 8–9.
- What `0x8226F050`'s return value is beyond an index — it is used at
  `820a7b1c` as a base for the parent lookup and its bound is `0x400`
  (`8226f080 cmpwi cr6,r11,0x400`), which is the 1024-entry limit, not read
  further.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
820a7b00–820a7b2c and 0x8226F050's prologue read here
```

No product code changed.
