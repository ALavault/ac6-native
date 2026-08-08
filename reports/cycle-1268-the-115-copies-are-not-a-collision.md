# Cycle 1268 — the 115 copies are not a collision

## Qualification

Delegated investigation on the canonical project; **every load-bearing
instruction and every corpus count below was re-checked here.** `default.xex`
SHA-256 `acc302c1…11bcde`; corpus `reports/logs/cycle-739-pac-mission-gate/`.
**No oracle pass was spent.**

## Established — both entries mount mode 1, through a route nobody had looked at

PAC entries 199 and 210 are not reached by the DPL mount that serves entries
9, 119 and 165. They are reached through an **SWG** node, and the mode is
hard-wired rather than passed:

```
821d2010  li  r10,0x1
821d2018  stw r30,0x10(r11)      ; the child node
821d201c  stw r29,0x14(r11)      ; the id base
821d2024  stw r10,0x18(r11)      ; +0x18 = MODE = 1
```

and `0x821D1620` — the one runtime-mode site of the established 16 / 36 / 1
split — reads exactly those fields into the thunk's registers:

```
821d16b8  lwz r3,0x10(r31)
821d16c0  lwz r5,0x18(r31)       ; thunk r5 = MODE = 1
821d16c4  lwz r4,0x14(r31)       ; thunk r4 = id base
821d16c8  bl  0x82335f18
```

Per the rule derived in cycle 1260 — `823408f8 or r5,r23,r23` /
`823408fc addi r23,r23,0x1` — a mode-1 mount computes `base + ordinal` and
**never reads the file's GIDX id**. The allocator `0x820F80C0` is called once
per child, so every wrapper gets a distinct base.

**So the 115 wrappers carrying `0x08000000` are not 115 colliding keys. Their
ids are discarded before the registry sees them, and the first-wins insert
derived in cycle 1255 never fires on them.**

## The control, and it could have failed three ways

The worker loop starts at child **1**, not 0:

```
821d1834  li     r30,0x1
821d1838  bl     0x82234e30      ; the child count
821d183c  cmpwi  cr6,r3,0x1
821d1840  ble    cr6,0x821d18b0  ; nothing to do with one child
821d1844  bl     0x820f80c0      ; a fresh id, once PER CHILD
```

because child 0 is the SWG descriptor. Checked against the corpus here:

| container | children | `SWG_00` at index 0 | mounted (count − 1) | wrappers carrying `0x08000000` |
|---|---:|---|---:|---:|
| `idx_0199/000_FHM` | 15 | **yes** | 14 | **14** |
| `idx_0210/002_FHM` | 102 | **yes** | 101 | **101** |
| `idx_0009` | 39 | no | — | — |
| `idx_0119` | 25 | no | — | — |
| `idx_0165` | 11 | no | — | — |

Three ways to fail: the two SWG containers could have lacked the descriptor;
the arithmetic could have missed by one; or a DPL-mounted container could have
carried one too. None did.

The delegated report adds four more agreements — the selector jump table at
`0x821B012C` mapping `sel 2 → 0xC7` with a single writer for the selector field,
`level + 0xD1 = 0xD2` as a fourth independent formula turning index 1 into an
entry the boot touched, `r5 = 2` matching entry 210's child 2, and the DPL
attribute table giving 199 and 210 the same class word `0x0010` where 9, 119 and
165 differ. **This is not the "it looks like the `0x0F000000` case" reasoning
that was refused in cycle 1264**; that had no control, and this has five.

## Not established, and one of them corrects the temptation

- **What `0x08000000` means in the file is still unknown.** Cycle 1264's story
  for the other duplicate — "the file value is the allocator's seed" — **does
  not transfer**: this allocator seeds at `0x0E000000`, not `0x08000000`. What
  is established is only that the value is discarded, which is enough to kill
  the collision and is not an explanation of the value.
- **`level == 1` for Mission 01 remains a corpus identification.** `0x820943B0`
  is a mode-dispatched field read whose writer was not traced. This is now the
  fourth formula agreeing on index 1; four agreements are not a derivation.
- Two `bl 0x821D1DD0` sites take their id from registers that were not chased.
  They cannot carry 199 or 210 — the exhaustive immediate scan and the
  exhaustive `bl 0x820943B0`+`addi` scan account for every way either value can
  be materialised — but they are unresolved for other ids.
- Neither owning object was shown to be instantiated during Mission 01 from
  code; the control is the boot corpus plus the agreements above.

## Where the duplicate question ends

Cycle 1256 measured 346 wrappers, 205 distinct ids and **141 extra copies**.

| copies | explanation |
|---:|---|
| 28 | mode-1 mount, allocator-assigned ids (cycle 1264) |
| 113 | mode-1 SWG mount, allocator-assigned ids (this cycle) |
| **0** | **remaining** |

**Every duplicate in the corpus is an artefact of reading file ids that retail
discards.** Mount order is not load-bearing for Mission 01, and the flat
extraction is exactly right — which is what cycle 1248 worried about and cycle
1255 could not settle without knowing which arm each container takes.

The two containers whose mounts *are* mode-0 — entry 119's children 0 and 1 —
carry distinct ids, so first-wins never fires there either.
