# Cycle 1199 — the meshes have names, and they are Mission 01's

## The last call

`0x82355318(stream, obj+0x10)`, the per-stream handler:

```
8235532c  addi r30,r29,0x20     ; an array at stream+0x20
82355330  lhz  r11,0xa(r29)     ;   of u16 [stream+0x0A] elements
8235534c  addi r30,r30,0x18     ;   stride 0x18
82355358  mulli r11,r11,0x18
82355360  addic. r11,r11,0x20   ; then, past it, a linked list:
82355368  lhz  r8,0x8(r11)      ;   node+0x08 flags, same 0x8000 guard
82355374  lwz  r10,0x4(r11)
82355380  lwz  r9,0x80(r28)     ;   [obj+0x90]
82355384  add  r10,r9,r10
82355388  stw  r10,0x4(r11)     ;   node+0x04 += [obj+0x90]
82355394  lwz  r10,0x0(r11)     ;   node+0x00 = relative offset to the next
8235539c  add  r11,r10,r11      ;   0 terminates
```

The relocation base is `[obj+0x90]` — and cycle 1196 established what lives
there. `this+0x90` is the end of the four sections, and the control that fixed
the `+0x30` constant found **printable bytes at exactly that offset in 537 of
537 files**. So `[obj+0x90]` is the base of a string table, and both `node+0x04`
here and `rec+0x20` in `0x823555D0` are offsets into it.

That closes cycle 1197's open item — "what `rec+0x20` points at" — which I had
left as relocated-by-something-unexplained.

## The control, and it is the discriminating kind

For every top-level record in the corpus, resolve `rec+0x20` against the derived
string base and require a printable NUL-terminated C string:

```
records                                     : 13,014
a printable C string at nameBase + rec+0x20 : 13,014  (100.0%)
out of bounds                               :      0
```

This one **can** fail, and would, on a wrong base: cycle 1196 already showed that
dropping the `+0x30` puts the same computation into binary data for 537 of 537
files. Two independent derivations — the section walk in `0x82350F08` and the
relocation base in `0x82355318` — meet on the same address, and the bytes there
are strings.

## What the names say

```
000_NDXR.ndxr  off=0x0000  'mapparts_m01_x_p1_sh_0032_0_O_OBJ'
               off=0x0030  'mapparts_m01_x_p1_sh_0033_0_O_OBJ'
               off=0x0060  'mapparts_m01_x_p1_sh_0034_0_O_OBJ'
               off=0x0090  'mapparts_m01_x_p1_sh_006_separate_0_7_0_O_OBJ'
               off=0x00c0  'mapparts_m01_x_p1_sh_006_separate_0_8_0_O_OBJ'
               off=0x00f0  'mapparts_m01_x_p1_sh_006_separate_0_9_0_O_OBJ'
```

The offsets step by `0x30`, so the string table is fixed-slot. **The meshes are
Mission 01 map parts and they identify themselves.** Every mesh in the corpus can
now be named from the retail structure alone, without a manifest and without an
oracle.

That is directly useful to step 2e, which is gated on proving an ownership edge
for the static environment: the candidate resources are no longer anonymous
blobs. It does **not** prove the edge — a name is not ownership, and the
fail-closed rule stands untouched until the edge itself is read out of the image.

## Where the NDXR chain now stands

| stage | address | what it does |
|---|---|---|
| recognise | `0x8234CA28` | type code = `u16` at `+0x08`; GIDX is `0x10` bytes in front |
| dispatch | `0x8234CB58` | `0x200` → `0x82350CA0` / `0x82350C50` |
| construct | `0x82350C50/CA0` | vtable `0x8201283C` / `0x820128B4`, size at `+0x08` |
| sequence | `0x82352B88` | vtable slots `+0x18`, `+0x10`, `+0x20`; two are `blr` |
| header | `0x82350F08` | four section extents; body at `buf + [+0x10] + 0x30` |
| records | `0x823556E0` → `0x823555D0` | `u16 [+0x0A]` records of `0x30` at `file+0x30` |
| streams | `0x82355468` | four pointers, stride table `0x82012C40`, count `[obj+0x98]` |
| chain | `0x82355318` | `0x18`-byte array, then a linked list; names via `[obj+0x90]` |

Eight stages, all derived, no measured format imported.

## Not established, stated plainly

- `0x8233EE40`, called per element of the `0x18`-byte array, and `0x8233EF88`,
  called on the stream at the end. Two calls remain unread.
- Which of the four stream pointers is position, normal or UV. Naming the meshes
  does not name their attributes.
- The stride `0x14` remains **derived but uncontrolled** (cycle 1198). Nothing
  here changes that.
- Whether the `0x18`-byte array and the linked list are the same kind of thing.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
13,014 / 13,014 records resolve to a printable C string
```

No product code changed. No retail bytes are committed — the names above are six
strings quoted as evidence, not an extraction.
