# Cycle 1213 — the load path closes to the entry point, and a `bl`-only negative is not evidence

Every cycle from 1194 to 1211 ended with the same caveat: *derived, but not proven
to execute at load.* This closes most of it and measures why the rest cannot be
closed statically.

## The boot chain terminates at the entry point

```
0x821F5E90   ← the XEX entry point
  └ 821f6024  bl 0x821D7D90     ← main
      └ 821d7d9c  bl 0x821D5EF8 ← the boot resource mount
```

Verified here: `bl 0x821d5ef8` has **exactly one** site in the image, and
`bl 0x821d7d90` has **exactly one**. Nothing selects the call — it is the second
instruction after `main`'s prologue, and the `beq` above it only chooses between
parsed `argv` and `(0,0)`, both arms converging on the same block.

**`0x821D5EF8` runs on every boot, unconditionally.** That anchors the DPL mount,
`ResourceManager::init`, and the NSXR registry population of cycles 1193 and 1209,
all of which hung on an unproven caller.

`main` also calls `0x821D6BD0` unconditionally at `821d7df0` — five further
`0x82335F18` mounts — then enters a frame loop closed by `b 0x821d7e34` that
never exits.

## The Mission 01 NTXR mounts, and why looking in the obvious place fails

`mode` is argument 2 at each `0x82335F18` site: the thunk shifts arguments by one
into `0x82340870`, where `823408ec rlwinm r26,r26,0x0,0x1f,0x1f` masks it to bit
0. The enumeration is **53 sites, complete** — 36 with `li r5,0x0`, 16 with
`li r5,0x1`, one loading from memory.

**The Mission 01 mounts are the six `mode = 0` sites in `0x820FA9C0`** —
`820faecc`, `820fafa4`, `820fb0fc`, `820fba24`, `820fba70`, `820fbab0` — all
passing `baseId = 0`, `mode = 0`, `arg3 = 0x40`. `mode & 1 == 0` means the id is
read from the entry, which is exactly what cycle 1209's 179-of-179 GIDX join
requires.

And the trap, worth recording because I would have fallen into it:

> **The nine `0x82335F18` sites sitting *directly* in the three unit-constructor
> functions all pass `mode = 1`.**

Byte-identical across all three (`82098b54/78/9c`, `8219d3cc/f0/414`,
`821a0eb8/edc/f00`): `baseId = 0x9201/0x9202/0x9203`, `mode = 1`, on members
`0x34/0x35/0x36` of a fixed global container. Reserved-id auxiliary packs.
**Anyone searching the unit constructors for a `mode = 0` site would have found
none and concluded the mission does not mount textures.**

## The NDXR path is reached, and it is conditional

```
0x82097560 / 0x8219BDD8 / 0x8219F8C0   (vtable slot +0x2C siblings)
  └ [vtbl 0x8205C9A4 + 0xEC] = 0x820FA9C0
      └ 820fb094  bl 0x82337C68
          └ 82337c8c  bl 0x82343010      (exactly one caller)
              └ 82343078  bl 0x8234CB58  (exactly one caller)
                  ├ 8234cb74  bl 0x8234CA28
                  └ 8234cbe4  bl 0x82350CA0
```

The bottom links are forced by uniqueness. The virtual call is pinned by a
control that could have failed three separate ways — **receiver-offset
matching**: the mission function loads its receiver at `this + K`, and the
constructor builds a `0x8205C9A4` sub-object at the same `K`, with the *same
distinctive instruction pair*:

| class | constructor | mission function |
|---|---|---|
| `0x82096FF0` | `addis r3,r31,0x3; subi r3,r3,0x6ae0` → **+0x29520** | `82097840/48`, same pair |
| `0x8219B948` | `8219bb38/3c` → **+0x29130** | `8219c0b8/c0`, same pair |
| `0x8219BB68` | `addis; addi r3,r3,0x5c00` → **+0x35C00** | `8219fba0/a8`, same pair |

Three different literal offsets, three matches. A wrong receiver would have
mismatched at least one.

**But it is guarded**, and the honest statement says so:

```
820fb064  bl 0x82234e08        ; container member lookup
820fb070  beq cr6,0x820fb120   ; member absent -> skip
820fb078  beq cr6,0x820fb120   ; lookup failed -> skip
820fb094  bl 0x82337C68
```

**The NDXR path runs at mission load iff the mission's container carries that
member.** Reached, conditional on container content — not "always runs".

The same three functions are the sole gateway to scenario parsing:
`0x82330158` ← `0x8232F198` ← `0x8232F380` ← `0x8232CCA0` ← `0x82309D20` ←
`0x82249718` ← the trio, every link unique or near-unique.

## Two routes to the unit placement that were outside my frame

I briefed that `0x8229ADF8` "has five call sites via `0x8229AF80`". Measured here:

| `bl 0x8229adf8` | `bl 0x8229af80` |
|---|---|
| `8229b0e8` in `0x8229AF80` | `822553dc`, `82270b54` |
| **`8229b2fc` in `0x8229B1B0`** | `8229cb98`, `8229cda4` |
| **`8229cb80` in `0x8229C920`** | `8230b1bc` |

The five addresses I listed are `0x8229AF80`'s call sites — a different function.
`0x8229ADF8` has **three** direct sites, and two of them do not go through
`0x8229AF80` at all. My cycle-1206 report phrased this correctly; the briefing
did not, and the sloppier phrasing hid two routes.

**The mechanism is a message pump, not a call graph.** `0x8229C920` is a handler
switching on `0x1`, `0x7D1`, `0x7D2`, `0x7D4`, `0x7D5`, installed at vtable slot
`+0x24` of six sibling vtables. On `0x7D1` it re-enters itself with `0x7D4`
(`8229c96c li r4,0x7d4`), which is the branch that reaches the placement. So
`0x7D4` is self-generated; the trigger is `0x7D1`.

**Not established, with a measured reason:** there are **206** virtual call sites
through slot `+0x24`, and only a handful set `r4` to a literal. The message code
is usually register-held, so the literal senders found (`82232aa8`, `8229d7e4`,
`822a2928`) are **known to be an incomplete enumeration** and are not presented as
the trigger set. Closing this needs dataflow the static dump does not provide, or
an oracle.

## The systemic finding, and it inverts this session's other lesson

| | count |
|---|---|
| direct `bl 0x…` | 36,472 |
| indirect `bctrl` | 7,827 |
| tail `bctr` | 652 |
| functions in the image | 8,247 |
| **functions reachable from `main` by direct `bl` alone** | **800** |

**Direct-call reachability covers about 10% of this program.**

Cycles 1193, 1195, 1196 and 1202 were each burned by *live code on a dead path* —
believing something runs when it does not. This is the mirror image: **declaring
live code unreachable, available here at a hit rate of roughly nine in ten.** A
`bl`-only reachability negative in this binary is not weak evidence; it is no
evidence.

Two techniques did the work and are worth keeping: **vtable resolution** (dump
`.rdata` as words, find the function inside a pointer run, find code materialising
a base in that run — that is the constructor and the delta is the slot), and
**receiver-offset matching**, which is checkable, cheap, and can fail.

## Not established, stated plainly

- What invokes slot `+0x2C` — 114 call sites through that slot across unrelated
  hierarchies, and the gap between the frame loop and `0x82199F68` is unclosed.
  It is closable by resolving the two `bctrl`s in `main`'s loop; that was not
  spent.
- Whether Mission 01's load sequence sends `0x7D1`, per the 206 dispatch sites.
- Whether members `0x7/0x8/0x9` of the container reaching `0x820FA9C0` are
  Mission 01's specifically. The function is generic and the container is passed
  in — a data question, not a code question.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
bl 0x821d5ef8: 1 site; bl 0x821d7d90: 1 site; bl 0x8229adf8: 3 sites — all re-measured here
```

No product code changed.
