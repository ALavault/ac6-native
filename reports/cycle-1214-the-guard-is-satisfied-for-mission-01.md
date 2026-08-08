# Cycle 1214 — the guard the NDXR path hangs on, and Mission 01 satisfies it

Cycle 1213 closed the load path but left one conditional: the NDXR loader runs
**iff the mission's container carries the member `0x820FB064` looks up**, and
called that a data question. This answers it, and names the part that stays an
inference.

## What the guard actually tests

The two helpers are indexed accessors over a five-word directory header:

```
0x82234DD0(dir, i)                    0x82234E08(dir, i)
  82234dd0  lwz r11,0x0(r3)             82234e08  lwz r11,0x0(r3)
  82234dd4  cmpw cr6,r4,r11  ; count      82234e0c  cmpw cr6,r4,r11
  82234de4  lwz r11,0xc(r3)  ; offsets    82234e1c  lwz r11,0x10(r3) ; sizes
  82234dec  lwzx r11,r10,r11 ;   [i]      82234e24  lwzx r3,r10,r11  ;   [i]
  82234df4  beq -> 0         ; 0 -> null  82234e28  blr
  82234df8  lwz r10,0x4(r3)  ; base
  82234dfc  add r3,r10,r11
```

So `+0x00` is a count, `+0x04` a base, `+0x0C` an offset table and `+0x10` a size
table. The guard is:

```
820fb050  bl 0x82234dd0     ; member pointer
820fb060  stw r11,0x2cdc(r29)
820fb064  bl 0x82234e08     ; member size
820fb070  beq cr6,0x820fb120 ;   pointer null  -> skip
820fb078  beq cr6,0x820fb120 ;   size zero     -> skip
820fb094  bl 0x82337C68      ; the NDXR path
```

**A member-existence test**, nothing more.

`0x820FA9C0` probes members by **literal index** and caches each pair:

```
820faaf4  li r4,0x4  -> stw r11,0x0c(r31) / stw r3,0x40(r31)
820fab2c  li r4,0x5  -> stw r11,0x10(r31) / stw r11,0x44(r31)
820fab50  li r4,0x6  -> stw r11,0x14(r31) / stw r11,0x48(r31)
820fab78  li r4,0x8  -> stw r11,0x18(r31) / stw r11,0x4c(r31)
820faba0  li r4,0x7  -> stw r11,0x1c(r31) / ...
```

pointers from `this+0x0C` upward, sizes from `this+0x40` upward.

## Mission 01's entry carries them

`idx_0009` has **26 members, indexed 000–025**, and members **4 through 10 are
FHM bundles**. Cycle 1213 placed the six `mode = 0` NTXR mounts on members
`0x7`, `0x8`, `0x9`.

## The control, and it could have failed

If the identification is right, the mount targets must be members that actually
hold textures. Census of all 26:

| members | contents |
|---|---|
| **4 – 10** | FHM, **exactly 2 `.ntxr` each**, 0 `.ndxr` |
| 18, 19 | FHM, 7 `.ntxr` each |
| **13, 17, 22, 23** | FHM, **0 `.ntxr`** — 12, 4, 544 and 279 other files |
| 0–3, 11, 12, 14–16, 20, 21, 24, 25 | single files (`MDLP`, `PLAD`, `ACE6`, …) |

**Members 7, 8 and 9 land inside the only uniformly texture-bearing band, and
four other FHM members carry no texture at all.** Had the mounts named 13 or 22,
the reading would have been wrong and this census would have said so. That is the
discrimination cycle 1213 could not supply from the code alone.

Also worth recording: **no member of Mission 01's entry holds a bare `.ndxr`.**
The geometry lives inside `001_MDLP.mdlp` and the two large bundles 22 and 23 —
consistent with the model binding running through the MDLP directory, which
cycles 1157/1174 ported.

## What stays an inference

**That the container `0x820FA9C0` is passed is Mission 01's archive entry.** The
shapes agree — an indexed member directory, literal indices 4 through 9, and an
entry that has exactly those members with exactly the right contents — but I did
not read the code that supplies it. This is the same standing as cycle 1206's
"Set 0 is the player's Set": convergent, not derived. **If the container is a
different one, the census above proves nothing about the guard.**

The honest form of cycle 1213's conclusion is therefore: the NDXR path is reached
and its guard is a member-existence test; Mission 01's archive entry carries
members with the required indices and the required contents; and the last link
between the two is unread.

## Not established, stated plainly

- Who supplies the container to `0x820FA9C0`.
- What `r26` — the index at the guard itself, initialised `li r26,0x0` at
  `820faa4c` — holds when the guard runs. The cached probes use literals; the
  guard uses a register.
- Whether members 4–10's uniform "2 NTXR" shape means anything, or is an artefact
  of how the extraction splits FHM bundles.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
26 members censused; 4 FHM members with zero textures available to falsify
```

No product code changed.
