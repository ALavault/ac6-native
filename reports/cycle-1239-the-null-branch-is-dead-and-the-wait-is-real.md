# Cycle 1239 — the NULL branch is dead, and the load-wait is real

Cycle 1235 closed with a reserve: the clear of the gate byte is skipped when the
mode-selected loader returns NULL, and *"which of the three runs for Mission 01,
and whether it can fail, is not established."* Both are now established, and the
reserve closes in the safe direction.

## The branch cannot be taken

All three arms — `0x821B8050` (mode 4), `0x821B8218` (mode 3), `0x821B7DC8`
(else) — return the value of `0x821B7960`, the mode-3 arm transitively through
the else arm it calls first. Each has exactly one definition of its return
register, and each returns it rather than any of the several other call results
it holds in non-volatile registers.

And `0x821B7960` is constant. Scanned over its whole `.pdata` extent, with the
denominator reported:

```
scanned=282  already_listed=281  forced=0  undisassemblable=1  hits=2
821b7d5c  li r3,0x1
821b7db8  li r3,0x1
```

**Two `li r3,` in 282 instructions, both loading 1.** No `li r3,0x0`, no `blr`,
two exits through `__restgprlr_26` (`0x82382F40`), which touches r26–r31, r12 and
LR and never r3.

So `r29` at `821b8404` is always 1, `821b8408 beq` is never taken, and
`821b8418 stb r30,0xa(r31)` always executes. **`0x82199F68` really is a load-wait
state**, and cycle 1235's reading survives its own reserve.

The control that could have failed: the same exit extractor run on `0x821B8430`
(Poll), a function already known to return 0 and −1, reports three distinct
return values. A zero return in `0x821B7960` was findable and is absent —
**3 values found there, 1 value twice here.**

## Which arm, and it is decided statically

**The `else` arm, `0x821B7DC8`.**

`CModeTaskGame`'s base constructor `0x82199BD8` normalises the mode word into
`{1, 2}`:

```
82199c5c  cmpwi cr6,r10,0x1
82199c60  beq   cr6,0x82199c70    ; already 1 -> keep
82199c64  cmpwi cr6,r10,0x2
82199c68  beq   cr6,0x82199c70    ; already 2 -> keep
82199c6c  stw   r9,0x78(r11)      ; else := 1
```

and the creator cycles 1218/1234 identified — `0x821BBF98` at `[0x82691B8C]` — is
twenty instructions long and contains exactly one call, to `0x82199BD8`. **No
derived override.** With the mode at 1 or 2, `cmpwi 4` and `cmpwi 3` both fail.

**The control here is the sharpest of the three.** The rival — *"entry 45
constructs a derived task that overwrites the mode, like its siblings"* — is a
shape that demonstrably exists: six sibling creators call `0x82199BD8` and then
immediately store 4, 5, 3, 5, 3, 3 into `+0x78`. Scored **0 of 1 for entry 45,
6 of 6 for the rival shape elsewhere.** The pattern was findable and this creator
does not have it.

## A correction to the ladder I wrote today

The two selectors are **not the same partition of the same word**:

| selector | tests | disagreement |
|---|---|---|
| mission manager (`8219a028`, `8219a0ac`) | **4**, then **5** | — |
| `BeginLoad` (`821b83dc`, `821b83ec`) | **4**, then **3** | modes **3** and **5** |

Mode 3 gets the campaign *manager* and the tutorial *load* arm; mode 5 gets the
replay manager and the campaign load arm. My ladder entry says "modes 1, 2 and 3
share the `else` arm" — true of the **manager** selector only, and it now says so.

## Can it fail in practice? No, and the real fragility is elsewhere

`0x821B7960` does perform lookups, and `0x821D2FC0` does return 0 on a miss
(`821d3048 li r3,0x0`) — **but it never returns one.** It resolves a container,
converts the mission name to ids, registers load requests, and returns a
compile-time constant. No property of a Mission 01 payload can make it return
NULL, because it does not return a lookup.

The genuine failure mode on this path is different and worth recording:
`821b8350 bl 0x821d2fc0` can return 0, and `821b83b8 stw r30,0x2c(r3)`
dereferences a second lookup's result **without a check**. That is a null
dereference, not a skipped clear.

## The trap the agent caught, and it is a new shape

A first writer scan reported `821c3970 stw r30,0x78(r11)` as a mode write on a
campaign path. It is not: `821c3960 lwz r11,0x4(r31)` **redefines `r11` six
instructions before the store**, and the `lwz r11,0x4eb4(r28)` the back-window
matched is 23 instructions further up.

**A displacement scan with a back-window and no clobber check manufactures
exactly the finding it is looking for.** Adding the check took 36 candidates to
35 and removed the only one pointing the wrong way — the one that would have made
the answer interesting.

## Not established, stated plainly

- What `[0x826E4EB4]` points to. No `stw *,0x4eb4(*)` exists in 100% of `.text`;
  six `addi` sites form its address, so it is written by a static initialiser or
  a split-constant store that was not scanned. The `+0x78` field could belong to
  a sub-object at `+0x70`.
- Five mode writers store non-literal values. All are in menu or network code and
  none is on the campaign path, but the values were not resolved.
- Whether `0x821A41D8`'s subtree writes the mode. A `bl` closure reaches 1,140
  functions and leaks through shared machinery, deciding nothing — and `bl`
  covers ~26% of this program anyway. **It does not affect the conclusion**: only
  the arm's side effects would change, never its return value.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
0x821B7960 scanned over its full 282-instruction extent: 2 hits, both li r3,0x1
```

No product code changed.
