# Cycle 1192 — FHM is not in the image at all, and cycle 1181 misread the registry

## Two corrections to my own reading

**Cycle 1181's "registry entries are twelve bytes each" is wrong.**
`0x82342D70(this, n)` is a **byte-size query for n entries**, not `entry_at(index)`.
Verified here instruction by instruction:

```
0x8234CD38  rlwinm r3,r4,0x4,0x0,0x1b   ; n * 0x10
0x8234CC80  mulli  r3,r4,0xc0           ; n * 0xC0
0x82342D9C  mulli  r10,r31,0xc          ; n * 0x0C
            summed                      ; n * 0xDC
```

I read `r31` as an index when it is a count, and the two calls as returning bases
when they return sizes. `0x82342F68` then carves that one block into three
parallel arrays — a pool of `0xC0`-byte elements, a map of `0x10`-byte nodes, and
a free list of `0xC`-byte nodes. **The twelve bytes are the free-list node**, one
array of three, and per-resource cost is `0xDC`.

**There is no type registry.** `0x82337BD8` has one caller, `0x821D5FE4`:

```
821d5fdc  li  r4,0x400
821d5fe0  lis r3,0x10        ; 1 MiB
821d5fe4  bl  0x82337bd8
```

It is `ResourceManager::init(heap = 1 MiB, maxResources = 0x400)`. Type dispatch
is **compiled in**, at `0x8234CA28` and `0x8234CB58`, over exactly three codes —
1 and 2 to `0x82350AF8`, `0x200` to the `ModelInfo20` constructors. No table, no
stored function pointers, nothing for a caller to register FHM with.

So cycle 1181's named next step — "whichever of the seven API functions writes an
entry" — was chasing something that does not exist.

## The hard negative

**The bytes `46 48 4D` — "FHM", not even requiring the trailing space — occur
zero times anywhere in the loaded image**, code or data. `.fhm` and `.FHM`
likewise. The scanner that returned zero found `NDXR` at `0x8200A24C`, `GIDX` at
`0x82067EC8` and `NTXR` at `0x82067EC0`, which is the known-good check
`INSTRUMENT_DISCIPLINE.md` demands before believing a zero.

No instruction carries `0x4d20`, the low half of `FHM `. The version word
`01 01 00 10` is never compared. Structural scans for the child-table idiom —
load a count, scale by 4, index two parallel tables — found nothing across three
displacement candidates.

**Retail does not parse an FHM container in this executable.** Task 2d is not
blocked; it is impossible, and that is a resolution rather than a defeat. The
measured layout in `tools/ac6_fhm.py` cannot be replaced by a reading, because
the reading does not exist to be found.

## What retail walks instead

Members come from the **DPL archive layer**, not the container. `0x821D5EF8`
mounts `sim:DATA.TBL`, `game:\DATA00.PAC` and `game:\DATA01.PAC`, and
`0x821CC4D0` walks members through two runtime tables:

```
821ccdb4  lwz   r10,-0x45c8(r24)   ; *0x8293BA38, a 4-byte-per-entry index
821ccdfc  lhz   r4,0x2(r11)        ;   second u16 of the slot
821cce80  lhzx  r11,r11,r9         ;   first u16 = first member number
821cce88  mulli r10,r11,0x44       ; member records are 0x44 bytes
821cce8c  lwz   r11,-0x45c4(r21)   ; *0x8293BA3C, the member record base
821cce94  addi  r5,r27,0x4         ; the member name at record+0x04
821ccf84  lhz   r10,0x2(r10)       ; the member count for this entry
```

A `{first member, member count}` index over a `0x44`-byte member table with names.
That is the structure a port should walk, and it is in the image.

## Not established, stated plainly

Where `0x8293BA38` and `0x8293BA3C` are populated from. `game-files/DATA.TBL` on
disk is 14,824 bytes — `u32 count = 926`, `u32 pacCount = 2`, then 926 sixteen-byte
records — and that shape is **not** the u16-pair / `0x44`-record shape
`0x821CC4D0` consumes. A build step sits between them and was not found. The two
writers of those globals are the next thing to chase.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
three live contracts                                ->  audit-valid
```

No product code changed.
