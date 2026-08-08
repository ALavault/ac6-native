# Cycle 1218 — the frame loop reaches the mission loader, in thirteen hops

Cycle 1213 closed the boot chain to the entry point and left one gap: *what
invokes slot `+0x2C`*. This closes it, and corrects a number I published in
`INSTRUMENT_DISCIPLINE.md` two cycles ago.

## Both of `main`'s indirect calls are dead ends

- **`821d7e48 [*0x82671308 + 0x0C]()`.** `0x82671308` has exactly one store in
  the image, at `0x823D298C` — a static initialiser **Ghidra's listing does not
  contain** (gap at `823D2964`–`823D29AC`). The value is `0x826E4F00`, whose
  vtable is `CNuMovie`, and slot `+0x0C` is `0x822DDBE8`, whose first instruction
  is `blr`. The sibling vtable `CAce6Movie` has the **same stub** at that slot.
  Both arms are empty: this is the FMV tick and it does nothing in this build.
- **`821d7e5c [*0x8269DCF0 + 0x2C]()`.** No store anywhere; the link-time value
  resolves to `CX360MotionRequestManage`, and slot `+0x2C` forwards to a job
  fence that spins on a critical section until two counters clear.

**The mission path runs entirely through `821d7e34 bl 0x821D7A90`.**

## The chain

| # | site | what happens |
|---|---|---|
| 1 | `821d7e34` | frame update, unconditional |
| 2 | `821D7C08`/`821D7C34` | `[*0x82765B88]->vt[+0x04]()`; both branches of the byte test reach it |
| 3 | `0x822AAFC8` | `CAce6TaskManager` pump — three intrusive lists at `this+0x210/+0x220/+0x230` |
| 4 | `0x821B99B8` | `CTaskModeManager` tick; registered at `821D6B4C` inside the boot function `0x821D5EF8` |
| 5 | `821BA1B4` | mode switch, when the counter at `this+0x1C` reaches 0 |
| 6 | `821BA974` | the creator pointer at `this+0x10`; creator table `0x82691AD8`, 175 entries, entry 45 = `0x821BBF98` = `new CModeTaskGame` |
| 7 | `822AB164` | `Register` **tail-calls** `task->vt[+0x0C]` |
| 8 | `0x82199D08` | builds the MSVC pointer-to-member `{0x82199F68,…}` and calls `CFsm::SetInitialState` with **−3 (ENTER)** |
| 9 | `0x82199F68` ENTER | creates the mission manager, stores it at `this+0x288` |
| 10 | `0x82199DA0` | per frame, calls the current state with **−2 (UPDATE)** |
| 11 | `82199F88` | transitions to `0x8219A140`, which gets **−3** |
| 12 | `0x8219A140` | jump table at `0x8219A174` on `msg+3` |
| 13 | **`8219A1B0`** | **the mission load call** |

Read here, instruction by instruction:

```
8219a1b0  lwz r3,0x288(r30)   ; the mission manager
8219a1b4  lwz r11,0x0(r3)     ; its vtable
8219a1b8  lwz r11,0x2c(r11)   ; slot +0x2C
8219a1c0  bctrl
```

And the selector, `mode = *([0x826E4EB4] + 0x78)`, also read here:

```
8219a024  lwz   r11,0x78(r11)
8219a028  cmpwi cr6,r11,0x4
8219a054  bl    0x82095fa8    ; mode 4 -> Online     -> slot +0x2C = 0x82097560
8219a05c  stw   r30,0x288(r31)
8219a0ac  cmpwi cr6,r11,0x5
8219a0d0  bl    0x821a35e0    ; mode 5 -> Replay     -> slot +0x2C = 0x8219BDD8
                              ; else   -> Campaign   -> slot +0x2C = 0x8219F8C0
```

**Modes 1, 2 and 3 all land on the `else` arm** — Campaign and Tutorial share
`0x8219F8C0`. That is a real ambiguity in the binary, not a gap in the trace.

## The control that could have failed two ways

Across the image there are **110** slot-`+0x2C` virtual call sites matching the
strict `lwz` / `mtspr CTR` / `bcctrl` idiom, and **exactly one** loads its
receiver from `[rX+0x288]`: `8219A1B8`. Zero would have meant the identification
was wrong; many would have meant it was useless.

A second, independent check: the three constructors `0x82199F68` invokes write
exactly the three vtables that carry `0x82097560` / `0x8219BDD8` / `0x8219F8C0`
at slot `+0x2C`. A call through `+0x28` or `+0x30` would have broken it.

## Correcting my own instrument note

`INSTRUMENT_DISCIPLINE.md` states that `main` reaches **800 of 8,247** functions
by direct `bl` — about 10%. A complete byte-level branch decode over 100% of
`.text` gives **2,144 of 8,135 by `bl` plus non-local `b`** — about **26%**.

The qualitative conclusion is unchanged and the mirror-trap warning stands: three
quarters of the program is still unreachable that way, and cycle 1213's findings
all held. But **the number was measured on Ghidra's 91.5% listing and is too
low**, and a discipline file that carries a wrong figure teaches the wrong margin.

## Two techniques to add

**MSVC RTTI recovery.** For each run of `.text` pointers, read the word at
`vtable−4` as a `RTTICompleteObjectLocator`, follow `+0x0C` to the type
descriptor, read the decorated name at `+8`. This yields **811 named vtables** —
independently matching the campaign's own long-standing 811 figure — and turns
every vtable in this report into a checkable class name.

**Tail calls through `bcctr`.** `CAce6TaskManager::Register` ends in `bcctr`, not
`bcctrl`, through slot `+0x0C` of its argument. **Every task registration hook in
this program — including hop 8, the entry to the entire mission chain — is
reached only through that one tail call.** A scan for `bcctrl` misses all of them.

## And a red herring retired

`0x8219AD20` is **not** the same mechanism as `0x82199F68`; the numeric adjacency
is coincidence. It has **51 direct `bl` callers** and is `CHsm` transition
machinery. The 36-state hierarchical machine this product ported lives *inside*
the mission manager — one level **below** the `+0x2C` call, not above it.

## Not established, stated plainly

- **The gate byte at `[0x8293BA10] + 0x15A946`**, which selects hop 11. It is set
  to 1 at `821B9508` and **no writer was found** — but a `stb` through a
  non-constant base is invisible to the scan used, and no positive control exists
  for that shape. If nothing clears it, the transition fires on the first UPDATE.
- **What selects creator index 45.** The table and the field holding the index
  are found; the code writing it is not. Only five instructions touch
  displacement `0x1AD8`, and `0x821C5E00`-ish is the next place to look.
- The 110-site figure counts the strict idiom only; a site with an instruction
  scheduled between the load and `mtspr` is missed. So the `[rX+0x288]`
  uniqueness is "unique among 110 measured", not "unique in the program".
- Whether `[*0x8269DCF0]`'s vtable is ever overwritten by a memcpy-style write.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
8219a1b0..c0 and the mode selector re-read here
```

No product code changed.
