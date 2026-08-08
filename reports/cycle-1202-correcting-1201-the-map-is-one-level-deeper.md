# Cycle 1202 — correcting cycle 1201: the map is one level deeper

## The correction

Cycle 1201 said `0x8233EBB0` reaches the map with `addi r3,r3,0x80`, and made
much of that `0x80` matching the map offset cycle 1192 derived from the
allocator. **The search is not there.** `0x8234AE00` adds another `0x80` before
searching:

```
8234ae48  addi r3,r31,0x80    ; r31 is already registry + 0x80
8234ae4c  bl   0x8234cf68     ; the actual search
```

So the map searched is at `registry + 0x100`, not `registry + 0x80`, and cycle
1201's sentence — "two derivations land on the same offset" — was true of a
number I had not finished following.

**The refined reading keeps the meeting and moves its base.** `registry + 0x80`
is the ResourceManager; the map is at *manager* `+0x80`, which is `registry +
0x100`. Cycle 1192's carve-up — pool at `+0x04`, map of `0x10`-byte nodes at
`+0x80`, free list at `+0x3C0` — is measured from the manager, and on that
reading it still agrees. What was wrong is that I called `0x828C8100` and
`0x828CCB80` managers. They are **containers with a manager at `+0x80`**.

I published that an hour ago after one disassembly, and one more disassembly
undid the framing. The finding survives; the sentence I wrote about it did not.

## Sixteen reserved ids that bypass the map

```
8234ae18  li     r11,-0x10               ; 0xFFFFFFF0
8234ae20  cmplw  cr6,r30,r11
8234ae24  blt    cr6,0x8234ae3c          ; below it -> the map
8234ae28  rlwinm r11,r30,0x0,0x1c,0x1f   ; key & 0xF
8234ae2c  lwz    r10,0x300(r31)          ; an array at manager + 0x300
8234ae30  mulli  r11,r11,0x120           ; slots of 0x120 bytes
8234ae34  add    r3,r11,r10
```

**Ids at or above `0xFFFFFFF0` never reach the map.** They index a sixteen-slot
array of `0x120`-byte entries. That array is at manager `+0x300`, which cycle
1192's carve-up did not mention — its `+0x3C0` is the free list, a different
thing — so the manager has structure beyond what the allocator reading showed.

**Control: zero of 13,014 corpus ids are sentinels.** So this is the fourth
live-but-unused branch this session, after cycles 1193, 1195 and 1196. Mission 01
resolves everything through the map.

The map search itself is `0x8234CF68`, taken under a lock — `0x823D6A7C` before,
`0x823D6A8C` after.

## The id namespace, measured

179 distinct ids over 13,014 references, and they split cleanly:

| high half | distinct ids |
|---|---|
| `0x0000` | **170** — small integers, `0x49E` to `0x7EB` |
| `0x1000` | **9** — `0x10001C56`, `0x10002715`, `0x10002718`, `0x10002815`, … |

The nine are the shape of a GIDX handle. `PLAYABLE_PLAN.md` records exactly one
pixel-decoded texture profile, **GIDX `0x10002215`** — same high half, same
width.

**I am not concluding they are the same namespace.** No GIDX artefact is
extracted in this workspace to check against (the search found zero files), and
"two numbers begin with `0x1000`" is a resemblance. It is a lead worth handing to
the material-link work, stated as a lead.

## Not established, stated plainly

- `0x8234CF68`, the map search, and the `0x10`-byte node layout it walks.
- What inserts into either map. Still the blocking unknown for the port.
- What the sixteen `0x120`-byte reserved slots are, and what fills them.
- Whether the 170 small ids and the 9 `0x1000`-form ids are one namespace with a
  tag or two namespaces sharing a field.
- Whether either container is the instance `0x821D5FE4` initialises.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
0 of 13,014 corpus ids are >= 0xFFFFFFF0
```

No product code changed.
