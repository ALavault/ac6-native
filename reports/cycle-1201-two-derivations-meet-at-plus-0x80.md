# Cycle 1201 — two independent derivations meet at `+0x80`

## The lookup

`0x8233EBB0(registry, key, out)` is five instructions of substance:

```
8233ebc0  addi   r3,r3,0x80    ; the map lives at registry + 0x80
8233ebc8  bl     0x8234ae00    ; map_find(map, key)
8233ebd0  cntlzw r10,r11
8233ebd4  rlwinm r3,r10,0x1b,0x1f,0x1f   ; r3 = (result == 0), the same branchless
8233ebd8  stw    r11,0x0(r31)  ; *out = result
```

The return convention is the `cntlzw` idiom cycle 1194 wrote up for the magic
predicates, used here as a null test: `r3` is 1 when the lookup **fails**. The
caller at `0x8233EE74` branches on `r3 == 0`, which is the found case.

## Why `+0x80` matters

Cycle 1192 derived the resource-manager layout from the other end — from the
allocator. `0x82342D70` sizes `n` entries at `n * 0xDC`, and `0x82342F68` carves
that one block into three parallel arrays:

> a pool of `0xC0`-byte elements at `+0x04`, **a map of `0x10`-byte nodes at
> `+0x80`**, and a free list of `0xC`-byte nodes at `+0x3C0`.

`0x8233EBB0` adds `0x80` to reach the map. **Two derivations made nine cycles
apart, from opposite ends — one reading how the memory is carved, one reading how
it is searched — land on the same offset.** Neither was fitted to the other; I did
not have `0x8233EBB0` in hand when 1192 was written, and cycle 1192's own closing
section was wrong about a different structure entirely.

So `0x828C8100` and `0x828CCB80` are **ResourceManager instances**, and the 179
ids of cycle 1200 are handles into a resource map — not offsets, not indices into
anything in the file.

## What this does and does not settle

It settles the *mechanism*. An NDXR stream names its data by an integer that is
looked up in a resource manager's map, which is why the geometry is not inline and
why no amount of reading the file will produce vertices.

It does **not** settle the 179. Cycle 1200 noted that 179 is also, separately, the
count of distinct NDXR models in `MISSION01_LADDER.md`, and refused to join them.
That refusal stands: knowing the ids are map keys tells me nothing about what the
map holds. The population side — what inserts into `0x828C8100`, and with what —
is unread.

`0x821D5FE4` calls `ResourceManager::init(1 MiB, 0x400 resources)`, and 179 is
below 1,024. That is consistency, not evidence, and I am recording it as the
former. I have not established that either registry is that instance.

## Not established, stated plainly

- `0x8234AE00`, the map search itself, and the `0x10`-byte node layout it walks.
- What inserts into either registry, and when relative to the NDXR load.
- Whether `0x828C8100` and `0x828CCB80` hold different resource kinds or are the
  same kind at different scopes. Two addresses and two call sites do not say.
- Everything cycle 1200 left open, unchanged.

## A note on the instrument

This cycle cost one disassembly. It was cheap because cycle 1192 had already
written the `+0x80` down — including the part of 1192 that was later corrected.
The correction in cycle 1193 struck the closing section, not the carve-up, and
keeping the two separable is what let this be a one-command confirmation instead
of a re-derivation. **Correcting a cycle in place, by section, is worth more than
retracting it whole.**

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
```

No product code changed.
